/* Native startup diagnostics. No stdio or allocation in
 * the signal handler. Raw context words retain evidence across SDK layouts.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <pthread_np.h>

static uintptr_t main_stack_low, main_stack_high;

extern void pemu_boot_write(const char *text, size_t length);
extern void pemu_boot_mark(const char *stage);
extern void _start(void *parameters, void (*teardown)(void));

static char *hex(char *out, uintptr_t value) {
    static const char digits[] = "0123456789abcdef";
    *out++ = '0'; *out++ = 'x';
    for (int shift = 60; shift >= 0; shift -= 4)
        *out++ = digits[(value >> shift) & 15];
    return out;
}

static void value(const char *key, uintptr_t number) {
    char line[96], *at = line;
    while (*key && at < line + 64) *at++ = *key++;
    at = hex(at, number);
    *at++ = '\n';
    pemu_boot_write(line, (size_t)(at - line));
}

static void report(int signal_number, siginfo_t *info, void *context) {
    value("BOOT_FAULT signal=", (uintptr_t)signal_number);
    value("BOOT_FAULT entry=", (uintptr_t)&_start);
    if (info) value("BOOT_FAULT address=", (uintptr_t)info->si_addr);
    if (context) {
        /* Both upstream and PS5 ucontext layouts contain at least this much
         * data. The PS5 SDK fork puts RIP/RSP at words 28/31; upstream FreeBSD
         * headers put them at 22/25. Record raw words instead of trusting the
         * installed SDK's offsets or changing the console's register state.
         * See mihawk-99/PS5_PayloadSDK include/freebsd/sys/_ucontext.h.
         */
        const uintptr_t *words = (const uintptr_t *)context;
        for (unsigned i = 0; i < 40; i += 4) {
            char line[112], *at = line;
            const char *prefix = "BOOT_CONTEXT ";
            while (*prefix) *at++ = *prefix++;
            *at++ = (char)('0' + i / 10);
            *at++ = (char)('0' + i % 10);
            *at++ = ':';
            for (unsigned j = 0; j < 4; ++j) {
                *at++ = ' ';
                at = hex(at, words[i + j]);
            }
            *at++ = '\n';
            pemu_boot_write(line, (size_t)(at - line));
        }
        /* Firmware 4.03 console context: RSP is word 31. Only inspect the
         * main stack whose bounds were obtained before installing handlers.
         * These are raw words, not an unwound call stack. Other threads skip. */
        uintptr_t sp = words[31];
        if (main_stack_high > main_stack_low && sp >= main_stack_low &&
            sp < main_stack_high && !(sp & (sizeof(uintptr_t) - 1))) {
            size_t count = (main_stack_high - sp) / sizeof(uintptr_t);
            if (count > 128) count = 128;
            value("BOOT_STACK start=", sp);
            for (size_t i = 0; i < count; ++i)
                value("BOOT_STACK word=", ((const volatile uintptr_t *)sp)[i]);
        } else {
            pemu_boot_mark("crash stack outside recorded main stack; skipped");
        }
    }
    /* SA_RESETHAND has restored the default action. Queue the same signal and
     * return so the console still produces its normal crash report. */
    raise(signal_number);
}

void pemu_boot_install_crash_report(void) {
    struct sigaction action = {0};
    const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    action.sa_sigaction = report;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    int failed = 0;
    for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        int result = sigaction(signals[i], &action, NULL);
        if (result != 0) {
            value("BOOT_REPORT install failed signal=", (uintptr_t)signals[i]);
            failed = 1;
        }
    }
    pemu_boot_mark("crash stack bounds begin");
    pthread_attr_t attr;
    void *base = NULL;
    size_t size = 0;
    if (pthread_attr_init(&attr) == 0) {
        if (pthread_attr_get_np(pthread_self(), &attr) == 0 &&
            pthread_attr_getstack(&attr, &base, &size) == 0 &&
            base && size && (uintptr_t)base <= UINTPTR_MAX - size) {
            main_stack_low = (uintptr_t)base;
            main_stack_high = main_stack_low + size;
            value("BOOT_STACK low=", main_stack_low);
            value("BOOT_STACK high=", main_stack_high);
        }
        pthread_attr_destroy(&attr);
    }
    pemu_boot_mark(failed ? "crash reporter incomplete" : "crash reporter installed");
}
