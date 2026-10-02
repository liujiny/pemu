/* Native filesystem imports. PS5_RetroArch records that native titles cannot
 * use its payload-style dlopen/dlsym path (tools/build-title.sh, 59a35aec4e).
 * Its frontend and PS5_PayloadSDK platform/directory.c instead import POSIX
 * chmod/getdents/lstat directly from libkernel. Keep the earliest logger free
 * of these calls; permission changes run only after main's crash reporter.
 */
#define _GNU_SOURCE 1
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

int getdents(int fd, char *buffer, int bytes);
extern void pemu_boot_mark(const char *stage);
extern int pemu_boot_fix_permissions(int (*change_mode)(const char *, unsigned int));

// Diagnostic formatting without stdio, allocation or dynamic API resolution.
static void mark_value(const char *name, uint64_t value)
{
    static const char hex[] = "0123456789abcdef";
    char line[128];
    unsigned int length = 0;
    while (*name && length < sizeof(line) - 20)
        line[length++] = *name++;
    line[length++] = '=';
    line[length++] = '0';
    line[length++] = 'x';
    for (int shift = 60; shift >= 0; shift -= 4)
        line[length++] = hex[(value >> shift) & 15];
    line[length] = 0;
    pemu_boot_mark(line);
}

static int change_log_mode(const char *path, unsigned int mode)
{
    // Use the SDK declaration: PS5 mode_t is 16 bits, unlike the host's.
    return chmod(path, (mode_t)mode);
}

int pemu_native_launch_init(void)
{
    pemu_boot_mark("native API direct imports");
    mark_value("API chmod", (uintptr_t)&chmod);
    mark_value("API getdents", (uintptr_t)&getdents);
    mark_value("API lstat", (uintptr_t)&lstat);

    pemu_boot_mark("log permissions begin");
    const int result = pemu_boot_fix_permissions(change_log_mode);
    const int saved_errno = errno;
    pemu_boot_mark(result == 0 ? "log permissions 0644 ok" : "log permissions failed");
    if (result != 0)
        mark_value("chmod errno", (unsigned int)saved_errno);
    // Permission failure must not stop the frontend or the early/crash logger.
    pemu_boot_mark("native API binding ok");
    errno = saved_errno;
    return 0;
}

int pemu_native_getdents(int fd, char *buffer, int bytes)
{
    return getdents(fd, buffer, bytes);
}

int pemu_native_lstat(const char *path, struct stat *status)
{
    return lstat(path, status);
}
