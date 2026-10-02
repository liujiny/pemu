/* Bounded native input-latency diagnostics. No input or rendering changes. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

extern void pemu_boot_mark(const char *stage);
extern void pemu_native_heap_mark(void);

/* Even phases begin, odd phases end: frame, poll, dispatch, presentation. */
static struct timing {
    uint64_t start, total, maximum, count;
    unsigned depth;
} stages[9];
static uint64_t window_start, last_poll, poll_gap_max;
static unsigned reports;

void pemu_native_input_probe(unsigned phase)
{
    if (phase >= 18 || reports >= 12) return;
    const int saved_errno = errno;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        errno = saved_errno;
        return;
    }
    const uint64_t now = (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    /* Ignore initialization polls before the measured frontend loop. */
    if (!window_start) {
        if (phase != 0) { errno = saved_errno; return; }
        window_start = now;
    }
    struct timing *t = &stages[phase / 2];
    if (!(phase & 1)) {
        if (!t->depth++) t->start = now;
        if (phase == 2) {
            const uint64_t gap = last_poll ? now - last_poll : 0;
            if (gap > poll_gap_max) poll_gap_max = gap;
            last_poll = now;
        }
    } else if (t->depth && !--t->depth) {
        const uint64_t elapsed = now - t->start;
        t->total += elapsed;
        if (elapsed > t->maximum) t->maximum = elapsed;
        ++t->count;
    }
    if (phase == 1 && now - window_start >= 5000000 && stages[0].count) {
        char line[192];
        const uint64_t frames = stages[0].count;
        snprintf(line, sizeof(line),
                 "INPUT_PERF frames=%llu window_us=%llu frame_avg_us=%llu frame_max_us=%llu",
                 (unsigned long long)frames, (unsigned long long)(now-window_start),
                 (unsigned long long)(stages[0].total/frames),
                 (unsigned long long)stages[0].maximum);
        pemu_boot_mark(line);
        snprintf(line, sizeof(line),
                 "INPUT_PERF poll_avg_us=%llu poll_gap_max_us=%llu dispatch_avg_us=%llu dispatch_max_us=%llu swap_avg_us=%llu",
                 (unsigned long long)(stages[1].count ? stages[1].total/stages[1].count : 0),
                 (unsigned long long)poll_gap_max,
                 (unsigned long long)(stages[2].count ? stages[2].total/stages[2].count : 0),
                 (unsigned long long)stages[2].maximum,
                 (unsigned long long)(stages[3].count ? stages[3].total/stages[3].count : 0));
        pemu_boot_mark(line);
        snprintf(line, sizeof(line),
                 "RENDER_PERF update_us=%llu clear_us=%llu draw_us=%llu",
                 (unsigned long long)(stages[4].count ? stages[4].total/stages[4].count : 0),
                 (unsigned long long)(stages[5].count ? stages[5].total/stages[5].count : 0),
                 (unsigned long long)(stages[6].count ? stages[6].total/stages[6].count : 0));
        pemu_boot_mark(line);
        snprintf(line, sizeof(line),
                 "RENDER_PERF submits=%llu submit_total_us=%llu submit_max_us=%llu setup_total_us=%llu",
                 (unsigned long long)stages[7].count,
                 (unsigned long long)stages[7].total,
                 (unsigned long long)stages[7].maximum,
                 (unsigned long long)stages[8].total);
        pemu_boot_mark(line);
        for (unsigned i=0; i<9; ++i) {
            stages[i].total = stages[i].maximum = stages[i].count = 0;
        }
        poll_gap_max = 0;
        window_start = now;
        ++reports;
        pemu_native_heap_mark();
    }
    errno = saved_errno;
}

/* Core-side mapped player-one state: at most 32 transitions per process.
 * This is diagnostic only and does not change controller state or mapping. */
void pemu_native_game_input(unsigned buttons)
{
    static unsigned previous, reports;
    if (reports >= 32 || buttons == previous) return;
    previous = buttons;
    ++reports;
    int saved_errno = errno;
    char text[] = "GAME_INPUT player=1 mapped=0x00000000";
    static const char digits[] = "0123456789abcdef";
    for (unsigned i = 0; i < 8; ++i)
        text[sizeof(text) - 2 - i] = digits[(buttons >> (4 * i)) & 15];
    pemu_boot_mark(text);
    errno = saved_errno;
}
