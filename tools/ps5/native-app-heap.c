// PS5 OpenGL - OpenGL implementation for PlayStation 5.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

/* Shared native-app allocator integration, originally used by the CTS runner. */
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *address, size_t size);
void __real_free(void *address);
int __real_posix_memalign(void **address, size_t alignment, size_t size);
size_t __real_malloc_usable_size(const void *address);

void *sceLibcMspaceCreate(const char *name, void *base, size_t size,
                          unsigned flags);
void *sceLibcMspaceMalloc(void *mspace, size_t size);
void *sceLibcMspaceCalloc(void *mspace, size_t count, size_t size);
void *sceLibcMspaceRealloc(void *mspace, void *address, size_t size);
void sceLibcMspaceFree(void *mspace, void *address);
int sceLibcMspacePosixMemalign(void *mspace, void **address, size_t alignment,
                              size_t size);
size_t sceLibcMspaceMallocUsableSize(const void *address);

/* The frontend/GPU keeps the r16 anonymous heap. Direct memory is only
 * allocated lazily for explicitly routed FBNeo CPU buffers, never GL objects. */
int64_t sceKernelGetDirectMemorySize(void);
int32_t sceKernelAllocateDirectMemory(int64_t, int64_t, size_t, size_t, int, int64_t *);
int32_t sceKernelMapDirectMemory(void **, size_t, int, int, int64_t, size_t);
int32_t sceKernelReleaseDirectMemory(int64_t, size_t);

static size_t ps5_heap_capacity;

static atomic_int ps5_heap_state;
static void *ps5_heap_base;
static void *ps5_heap_mspace;
/* Owned-heap usable bytes only: not GPU mappings, foreign heaps or process RSS.
 * Relaxed snapshots are observations, not an allocator synchronization fence. */
static atomic_size_t ps5_heap_live_bytes, ps5_heap_peak_bytes, ps5_heap_blocks;
static atomic_size_t ps5_heap_failures, ps5_heap_ambiguous_zero_reallocs;

extern void pemu_boot_mark(const char *stage);
static atomic_uint pemu_heap_failure_reports;
/* Fixed stack formatting only: this is also callable on allocator failure. */
static char *pemu_heap_hex(char *out, size_t v) {
  static const char digits[] = "0123456789abcdef";
  *out++ = '0'; *out++ = 'x';
  for (int shift = 60; shift >= 0; shift -= 4) *out++ = digits[(v >> shift) & 15];
  return out;
}
void pemu_native_heap_mark(void) {
  int saved = errno;
  char line[180], *at = line;
#define FIELD(label, value) do { const char *p = label; while (*p) *at++ = *p++; at = pemu_heap_hex(at, value); } while (0)
  FIELD("HEAP capacity=", ps5_heap_capacity);
  FIELD(" live=", atomic_load_explicit(&ps5_heap_live_bytes, memory_order_relaxed));
  FIELD(" peak=", atomic_load_explicit(&ps5_heap_peak_bytes, memory_order_relaxed));
  FIELD(" failures=", atomic_load_explicit(&ps5_heap_failures, memory_order_relaxed));
  *at = 0;
  pemu_boot_mark(line);
#undef FIELD
  errno = saved;
}
static void pemu_heap_allocation_failed(size_t bytes) {
  if (atomic_fetch_add_explicit(&pemu_heap_failure_reports, 1, memory_order_relaxed) >= 4) return;
  int saved = errno;
  char line[64] = "HEAP allocation failed bytes=";
  char *at = line; while (*at) ++at;
  *pemu_heap_hex(at, bytes) = 0;
  pemu_boot_mark(line);
  pemu_native_heap_mark();
  errno = saved;
}

static void ps5_heap_resize_stats(size_t before, size_t after) {
  size_t live = after >= before
      ? atomic_fetch_add_explicit(&ps5_heap_live_bytes, after - before,
                                  memory_order_relaxed) + after - before
      : atomic_fetch_sub_explicit(&ps5_heap_live_bytes, before - after,
                                  memory_order_relaxed) - (before - after);
  size_t peak = atomic_load_explicit(&ps5_heap_peak_bytes, memory_order_relaxed);
  while (live > peak && !atomic_compare_exchange_weak_explicit(
      &ps5_heap_peak_bytes, &peak, live, memory_order_relaxed, memory_order_relaxed)) {}
}

static void *ps5_heap_record_allocation(void *address, size_t bytes) {
  if (address) {
    ps5_heap_resize_stats(0, sceLibcMspaceMallocUsableSize(address));
    atomic_fetch_add_explicit(&ps5_heap_blocks, 1, memory_order_relaxed);
  } else if (bytes) {
    atomic_fetch_add_explicit(&ps5_heap_failures, 1, memory_order_relaxed);
    pemu_heap_allocation_failed(bytes);
  }
  return address;
}

static int ps5_heap_ready(void) {
  int state = atomic_load_explicit(&ps5_heap_state, memory_order_acquire);
  if (state == 2)
    return 1;
  if (state != 0)
    return 0;

  int expected = 0;
  if (!atomic_compare_exchange_strong_explicit(
          &ps5_heap_state, &expected, 1, memory_order_acq_rel,
          memory_order_acquire))
    return expected == 2;

  const size_t candidates[] = {256u * 1024u * 1024u, 128u * 1024u * 1024u};
  for (unsigned i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
    const size_t bytes = candidates[i];
    void *base = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANON, -1, 0);
    if (base == MAP_FAILED) {
      int error = errno;
      char line[128] = "HEAP mmap failed bytes=";
      char *at = line; while (*at) ++at;
      at = pemu_heap_hex(at, bytes);
      const char *key = " errno="; while (*key) *at++ = *key++;
      *pemu_heap_hex(at, (size_t)error) = 0;
      pemu_boot_mark(line);
      continue;
    }
    void *space = sceLibcMspaceCreate("PS5-OpenGL", base, bytes, 0);
    if (!space) {
      pemu_boot_mark("HEAP mspace create failed; trying smaller heap");
      munmap(base, bytes);
      continue;
    }
    ps5_heap_base = base;
    ps5_heap_mspace = space;
    ps5_heap_capacity = bytes;
    atomic_store_explicit(&ps5_heap_state, 2, memory_order_release);
    pemu_boot_mark(i == 0 ? "HEAP ready capacity=256MiB" : "HEAP ready capacity=128MiB");
    return 1;
  }
  atomic_store_explicit(&ps5_heap_state, -1, memory_order_release);
  pemu_boot_mark("HEAP initialization failed; stopping before frontend");
  _Exit(EXIT_FAILURE);
}

static int ps5_heap_owns(const void *address) {
  /* Acquire publication before reading non-atomic heap metadata. */
  if (atomic_load_explicit(&ps5_heap_state, memory_order_acquire) != 2)
    return 0;
  uintptr_t value = (uintptr_t)address;
  uintptr_t base = (uintptr_t)ps5_heap_base;
  return value >= base && value - base < ps5_heap_capacity;
}

static atomic_int core_heap_state;
static void *core_heap_base, *core_heap_mspace;
static size_t core_heap_capacity;

static int core_heap_owns(const void *address) {
  if (atomic_load_explicit(&core_heap_state, memory_order_acquire) != 2) return 0;
  uintptr_t value = (uintptr_t)address, base = (uintptr_t)core_heap_base;
  return value >= base && value - base < core_heap_capacity;
}
static int core_heap_ready(void) {
  int state = atomic_load_explicit(&core_heap_state, memory_order_acquire);
  if (state == 2) return 1;
  if (state != 0) return 0;
  int expected = 0;
  if (!atomic_compare_exchange_strong_explicit(&core_heap_state, &expected, 1,
      memory_order_acq_rel, memory_order_acquire)) return expected == 2;
  pemu_boot_mark("CORE_HEAP direct allocation begin");
  const size_t bytes = 256u * 1024u * 1024u;
  /* WB_ONION=0. Type 3 is WC_GARLIC, unsuitable for random CPU reads.
   * Do not share the rendering runtime's write-combined memory policy. */
  const int cpu_writeback_memory = 0;
  int64_t physical = 0, limit = sceKernelGetDirectMemorySize();
  if (limit >= (int64_t)bytes &&
      sceKernelAllocateDirectMemory(0, limit, bytes, 0x4000, cpu_writeback_memory, &physical) == 0) {
    void *base = NULL;
    if (sceKernelMapDirectMemory(&base, bytes, PROT_READ | PROT_WRITE,
                                0, physical, 0x4000) == 0 && base) {
      void *space = sceLibcMspaceCreate("pEMU-Core", base, bytes, 0);
      if (space) {
        core_heap_base = base; core_heap_mspace = space; core_heap_capacity = bytes;
        atomic_store_explicit(&core_heap_state, 2, memory_order_release);
        pemu_boot_mark("CORE_HEAP ready capacity=256MiB type=0 CPU writeback");
        return 1;
      }
      if (munmap(base, bytes) != 0) {
        pemu_boot_mark("CORE_HEAP unmap failed; retaining pages");
        atomic_store_explicit(&core_heap_state, -1, memory_order_release);
        return 0;
      }
    }
    if (sceKernelReleaseDirectMemory(physical, bytes) != 0)
      pemu_boot_mark("CORE_HEAP release failed");
  }
  atomic_store_explicit(&core_heap_state, -1, memory_order_release);
  pemu_boot_mark("CORE_HEAP unavailable; using main heap");
  return 0;
}

void *__wrap_malloc(size_t size);
/* Only BurnMalloc and state buffers opt in. Texture/driver allocations never
 * enter this pool, even after game load. The existing free/realloc wrappers
 * retain allocator ownership for every returned pointer. */
void *pemu_native_core_malloc(size_t size) {
  if (size >= 1024u * 1024u && core_heap_ready()) {
    void *p = sceLibcMspaceMalloc(core_heap_mspace, size);
    if (p) return p;
    pemu_boot_mark("CORE_HEAP full; trying main heap");
  }
  return __wrap_malloc(size);
}

void *__wrap_malloc(size_t size) {
  return ps5_heap_ready()
      ? ps5_heap_record_allocation(sceLibcMspaceMalloc(ps5_heap_mspace, size), size)
      : __real_malloc(size);
}

void *__wrap_calloc(size_t count, size_t size) {
  return ps5_heap_ready()
      ? ps5_heap_record_allocation(sceLibcMspaceCalloc(ps5_heap_mspace, count, size),
                                   count && size > SIZE_MAX / count ? SIZE_MAX : count * size)
      : __real_calloc(count, size);
}

void *__wrap_realloc(void *address, size_t size) {
  if (address == NULL)
    return __wrap_malloc(size);
  if (core_heap_owns(address)) {
    void *result = sceLibcMspaceRealloc(core_heap_mspace, address, size);
    if (!result && size) pemu_heap_allocation_failed(size);
    return result;
  }
  if (!ps5_heap_owns(address))
    return __real_realloc(address, size);
  size_t before = sceLibcMspaceMallocUsableSize(address);
  void *result = sceLibcMspaceRealloc(ps5_heap_mspace, address, size);
  if (result)
    ps5_heap_resize_stats(before, sceLibcMspaceMallocUsableSize(result));
  else if (size) {
    atomic_fetch_add_explicit(&ps5_heap_failures, 1, memory_order_relaxed);
    pemu_heap_allocation_failed(size);
  }
  else
    /* Preserve platform realloc(p,0) behavior; NULL does not tell us whether
     * p was freed. Mark the counters inconclusive instead of guessing. */
    atomic_fetch_add_explicit(&ps5_heap_ambiguous_zero_reallocs, 1, memory_order_relaxed);
  return result;
}

void __wrap_free(void *address) {
  if (core_heap_owns(address)) {
    sceLibcMspaceFree(core_heap_mspace, address);
    return;
  }
  if (ps5_heap_owns(address)) {
    ps5_heap_resize_stats(sceLibcMspaceMallocUsableSize(address), 0);
    atomic_fetch_sub_explicit(&ps5_heap_blocks, 1, memory_order_relaxed);
    sceLibcMspaceFree(ps5_heap_mspace, address);
  } else
    __real_free(address);
}

int __wrap_posix_memalign(void **address, size_t alignment, size_t size) {
  if (!ps5_heap_ready())
    return __real_posix_memalign(address, alignment, size);
  int result = sceLibcMspacePosixMemalign(ps5_heap_mspace, address, alignment, size);
  if (result == 0)
    ps5_heap_record_allocation(*address, size);
  else if (result == ENOMEM) {
    atomic_fetch_add_explicit(&ps5_heap_failures, 1, memory_order_relaxed);
    pemu_heap_allocation_failed(size);
  }
  return result;
}

size_t __wrap_malloc_usable_size(const void *address) {
  return (ps5_heap_owns(address) || core_heap_owns(address)) ? sceLibcMspaceMallocUsableSize(address)
                                : __real_malloc_usable_size(address);
}

void ps5_opengl_heap_stats_print(unsigned iteration) {
  if (iteration == 0) {
    int state = atomic_load_explicit(&ps5_heap_state, memory_order_acquire);
    printf("[ps5-opengl-cts] mspace state=%d base=%p size=%zu\n",
           state, state == 2 ? ps5_heap_base : NULL, ps5_heap_capacity);
  }
}

/* The native import converter rejects unresolved weak application symbols.
 * A diagnostic build's strong definition overrides this default no-op. */
__attribute__((weak)) void ps5_opengl_gpu_snapshot(const char *phase, unsigned iteration) {
  (void)phase;
  (void)iteration;
}
void ps5_opengl_heap_snapshot(const char *phase, unsigned iteration) {
  ps5_opengl_gpu_snapshot(phase, iteration);
  printf("[ps5-opengl-heap] phase=%s sample=%u state=%d live_bytes=%zu peak_bytes=%zu "
         "blocks=%zu failures=%zu ambiguous_zero_reallocs=%zu\n", phase, iteration,
         atomic_load_explicit(&ps5_heap_state, memory_order_acquire),
         atomic_load_explicit(&ps5_heap_live_bytes, memory_order_relaxed),
         atomic_load_explicit(&ps5_heap_peak_bytes, memory_order_relaxed),
         atomic_load_explicit(&ps5_heap_blocks, memory_order_relaxed),
         atomic_load_explicit(&ps5_heap_failures, memory_order_relaxed),
         atomic_load_explicit(&ps5_heap_ambiguous_zero_reallocs, memory_order_relaxed));
}

/* The FBNeo state codec is non-reentrant. Its one large snapshot is temporary,
 * not part of the game/UI heap; release it after save/load completes. */
static void *state_memory;
static size_t state_memory_bytes;
static int64_t state_memory_physical;
void *pemu_native_state_malloc(size_t size) {
  if (size < 1024u * 1024u) return __wrap_malloc(size);
  if (state_memory || size > SIZE_MAX - 0x3fff) { errno = ENOMEM; return NULL; }
  const size_t bytes = (size + 0x3fff) & ~(size_t)0x3fff;
  int64_t physical = 0, limit = sceKernelGetDirectMemorySize();
  if (limit < 0 || bytes > (uint64_t)limit ||
      sceKernelAllocateDirectMemory(0, limit, bytes, 0x4000, 0, &physical) != 0) {
    pemu_boot_mark("STATE_BUFFER direct allocation failed");
    errno = ENOMEM; return NULL;
  }
  void *base = NULL;
  if (sceKernelMapDirectMemory(&base, bytes, PROT_READ | PROT_WRITE,
                              0, physical, 0x4000) != 0 || !base) {
    if (sceKernelReleaseDirectMemory(physical, bytes) != 0)
      pemu_boot_mark("STATE_BUFFER map failure cleanup failed");
    pemu_boot_mark("STATE_BUFFER map failed");
    errno = ENOMEM; return NULL;
  }
  state_memory = base; state_memory_bytes = bytes; state_memory_physical = physical;
  char line[80] = "STATE_BUFFER ready bytes=";
  char *at = line; while (*at) ++at;
  *pemu_heap_hex(at, bytes) = 0; pemu_boot_mark(line);
  return base;
}
void pemu_native_state_free(void *address) {
  if (!address || address != state_memory) { __wrap_free(address); return; }
  if (munmap(state_memory, state_memory_bytes) != 0) {
    pemu_boot_mark("STATE_BUFFER unmap failed; retaining allocation");
    return;
  }
  // Clear the virtual address before release; never hand it to another heap.
  state_memory = NULL;
  if (sceKernelReleaseDirectMemory(state_memory_physical, state_memory_bytes) != 0) {
    pemu_boot_mark("STATE_BUFFER physical release failed");
  } else {
    pemu_boot_mark("STATE_BUFFER released");
  }
  state_memory_bytes = 0;
}
