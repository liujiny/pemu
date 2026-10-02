/* Link with --wrap=chmod/getdents/lstat. Exercise the direct POSIX
 * bridges, actual errno behavior and nonfatal permission errors. No fake
 * dlsym or module handle assumptions: the native implementation uses neither. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

int pemu_native_launch_init(void);
int pemu_native_getdents(int, char *, int);
int pemu_native_lstat(const char *, struct stat *);

static int permission_error, getdents_error, lstat_error;
static int permissions, permission_failure_marks, permission_success_marks;
static char last_mark[128];

void pemu_boot_mark(const char *stage)
{
    assert(strlen(stage) < sizeof(last_mark));
    strcpy(last_mark, stage);
    permission_failure_marks += !strcmp(stage, "log permissions failed");
    permission_success_marks += !strcmp(stage, "log permissions 0644 ok");
    // Diagnostics may modify errno; bridges must preserve the syscall result.
    errno = EDOM;
}

int __wrap_chmod(const char *path, mode_t mode)
{
    assert(!strcmp(path, "/app0/pemu_boot.log") && mode == 0644);
    permissions++;
    errno = permission_error;
    return permission_error ? -1 : 0;
}

int pemu_boot_fix_permissions(int (*function)(const char *, unsigned int))
{
    assert(!strcmp(last_mark, "log permissions begin"));
    return function("/app0/pemu_boot.log", 0644);
}

int __wrap_getdents(int fd, char *buffer, int bytes)
{
    assert(fd == 7 && buffer && bytes == 32);
    errno = getdents_error;
    return getdents_error ? -1 : 12;
}

int __wrap_lstat(const char *path, struct stat *status)
{
    assert(!strcmp(path, ".") && status);
    errno = lstat_error;
    status->st_ino = 123;
    return lstat_error ? -1 : 0;
}

int main(void)
{
    assert(pemu_native_launch_init() == 0 && permissions == 1);
    assert(!strcmp(last_mark, "native API binding ok"));
    permission_error = EACCES;
    assert(pemu_native_launch_init() == 0 && permissions == 2 && errno == EACCES);
    assert(permission_success_marks == 1 && permission_failure_marks == 1);

    char buffer[32];
    assert(pemu_native_getdents(7, buffer, sizeof(buffer)) == 12);
    getdents_error = EINTR;
    assert(pemu_native_getdents(7, buffer, sizeof(buffer)) == -1 && errno == EINTR);
    struct stat status;
    assert(pemu_native_lstat(".", &status) == 0 && status.st_ino == 123);
    lstat_error = ENOENT;
    assert(pemu_native_lstat(".", &status) == -1 && errno == ENOENT);
    puts("PASS direct native APIs: arguments, POSIX return/errno, nonfatal permissions, diagnostic errno preservation");
    return 0;
}
