/*
 * ps5-native-app-boilerplate - Native application startup.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PS5 pEMU keeps the PS5_RetroArch BSS fix here because the console loader does
 * not reliably zero the writable segment tail before _start executes.
 */

#include <cstddef>
#include <cstdint>
#include <fcntl.h>

using Destructor = void (*)();
using Initializer = void (*)();

extern "C"
{
    int sceKernelOpen(const char *path, int flags, unsigned int mode);
    long sceKernelWrite(int fd, const void *buffer, std::size_t length);
    int sceKernelClose(int fd);
    void pemu_boot_install_crash_report();
    int pemu_native_launch_init();
    void _init_env(void *process_parameters);
    int atexit(Destructor callback);
    [[noreturn]] void _Exit(int status);
    int sceSystemServiceLoadExec(const char *, const char *[]);
    int sceKernelUsleep(unsigned int);
    int application_main(int argc, char **argv, char **envp) __asm__("main");

    extern char __bss_start[];
    extern char __bss_end[];

    extern Initializer __preinit_array_start[] __attribute__((weak));
    extern Initializer __preinit_array_end[] __attribute__((weak));
    extern Initializer __init_array_start[] __attribute__((weak));
    extern Initializer __init_array_end[] __attribute__((weak));
    extern Initializer __fini_array_start[] __attribute__((weak));
    extern Initializer __fini_array_end[] __attribute__((weak));
}

namespace
{
// Loader-bound libkernel imports; no application libc, heap or BSS state.
int open_boot_log() noexcept
{
    long fd = sceKernelOpen("/app0/pemu_boot.log",
                          O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
        fd = sceKernelOpen("/download0/pemu_boot.log",
                          O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
        fd = sceKernelOpen("/data/pemu_boot.log",
                          O_WRONLY | O_CREAT | O_APPEND, 0644);
    return static_cast<int>(fd);
}

void boot_trace(const char *text, std::size_t length) noexcept
{
    const int fd = open_boot_log();
    if (fd < 0) return;
    // Keep the first write independent of BSS, libc and optional APIs. r2
    // reached main with these three libkernel imports on firmware 4.03.
    while (length) {
        const long written = sceKernelWrite(static_cast<int>(fd), text, length);
        if (written <= 0) break;
        text += written; length -= static_cast<std::size_t>(written);
    }
    (void)sceKernelClose(static_cast<int>(fd));
}
#define BOOT_STAGE(text) boot_trace("BOOT_STAGE " text "\n", sizeof("BOOT_STAGE " text "\n") - 1)

void zero_bss() noexcept
{
    for (volatile char *at = __bss_start; at != __bss_end; ++at)
        *at = 0;
}

void run_forward(Initializer *first, Initializer *last) noexcept
{
    if (first == nullptr || last == nullptr)
        return;
    while (first != last)
        (*first++)();
}

void run_reverse(Initializer *first, Initializer *last) noexcept
{
    if (first == nullptr || last == nullptr)
        return;
    while (last != first)
        (*--last)();
}
} // namespace

extern "C" void pemu_boot_write(const char *text, std::size_t length)
{
    boot_trace(text, length);
}

// Native title data and resources use the mounted title directly. Do not
// change the process cwd: firmware 4.03 returns EPERM for chdir("/app0").
extern "C" const char *pemu_native_data_path()
{
    BOOT_STAGE("native data path /app0/");
    return "/app0/";
}

// Run once after main's crash reporter is installed and an API has resolved.
// No permission callback is stored or invoked by the early/crash log writer.
extern "C" int pemu_boot_fix_permissions(int (*change_mode)(const char *, unsigned int))
{
    if (!change_mode) return -1;
    // A creation mask may have left the file at 0000. Path-based chmod can
    // repair that owned file even when it cannot be reopened for fchmod.
    int result = change_mode("/app0/pemu_boot.log", 0644);
    if (result != 0) result = change_mode("/download0/pemu_boot.log", 0644);
    if (result != 0) result = change_mode("/data/pemu_boot.log", 0644);
    return result;
}

extern "C" void pemu_boot_mark(const char *stage)
{
    char line[192];
    constexpr char prefix[] = "BOOT_STAGE ";
    std::size_t length = 0;
    for (char c : prefix) {
        if (c) line[length++] = c;
    }
    while (*stage && length < sizeof(line) - 1) line[length++] = *stage++;
    line[length++] = '\n';
    boot_trace(line, length);
}

extern "C" void _init()
{
    run_forward(__preinit_array_start, __preinit_array_end);
    run_forward(__init_array_start, __init_array_end);
}

extern "C" void _fini()
{
    run_reverse(__fini_array_start, __fini_array_end);
}

extern "C" [[noreturn]] __attribute__((visibility("default"))) void
_start(void *process_parameters, Destructor loader_teardown)
{
    BOOT_STAGE("build cv-thread-r21-20261002");
    BOOT_STAGE("_start entered");
    const int argc = *static_cast<const int *>(process_parameters);
    auto *parameters = static_cast<std::uint8_t *>(process_parameters);
    auto **argv = reinterpret_cast<char **>(parameters + sizeof(std::uint64_t));

    /*
     * This must happen before _init_env or any static constructor. pEMU, SDL and
     * Mesa carry substantially more zero-initialized state than the boilerplate
     * demo, so stale BSS bytes can terminate or corrupt startup before logging.
     */
    zero_bss();
    BOOT_STAGE("bss ok");

    _init_env(process_parameters);
    BOOT_STAGE("env ok");
    if (loader_teardown != nullptr)
        (void)atexit(loader_teardown);
    (void)atexit(_fini);
    BOOT_STAGE("constructors begin");
    _init();
    BOOT_STAGE("constructors ok");
    BOOT_STAGE("main calling");
    const int status = application_main(argc, argv, nullptr);
    BOOT_STAGE("main returned");
    // Ask the application manager to leave the title, rather than terminating
    // it behind the launcher's back with libc _Exit (r16 still reports a crash).
    BOOT_STAGE("system service exit request");
    const int exit_result = sceSystemServiceLoadExec("exit", nullptr);
    if (exit_result >= 0) {
        BOOT_STAGE("system service exit accepted");
        for (;;) sceKernelUsleep(100000);
    }
    BOOT_STAGE("system service exit failed; process exit fallback");
    _Exit(status);

}

// --wrap=main reaches this native entry before the existing pEMU main.
extern "C" int __real_main(int, char **, char **);
extern "C" int __wrap_main(int argc, char **argv, char **envp)
{
    BOOT_STAGE("main entered");
    pemu_boot_install_crash_report();
    if (pemu_native_launch_init() != 0) {
        BOOT_STAGE("native filesystem APIs unavailable");
        return 1;
    }
    return __real_main(argc, argv, envp);
}
