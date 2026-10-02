/* Host regression tests for native-directory.c. Linux supplies directory data;
 * the adapter below translates it into the console's getdents record format.
 * This validates the walker/parser, not the PS5 kernel ABI or mount policy. */
#define _GNU_SOURCE 1
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

DIR *__wrap_opendir(const char *);
DIR *__wrap_fdopendir(int);
struct dirent *__wrap_readdir(DIR *);
void __wrap_rewinddir(DIR *);
int __wrap_dirfd(DIR *);
int __wrap_closedir(DIR *);
char *__wrap_getcwd(char *, size_t);

static int fixture, interrupted;

int pemu_native_lstat(const char *path, struct stat *status)
{
    return lstat(path, status);
}

int pemu_native_getdents(int fd, char *buffer, int bytes)
{
    if (interrupted) {
        interrupted = 0;
        errno = EINTR;
        return -1;
    }
    if (fixture) {
        memset(buffer, 0, bytes);
        uint32_t ino = 1;
        uint16_t len = 16;
        memcpy(buffer, &ino, 4);
        memcpy(buffer + 4, &len, 2);
        buffer[6] = DT_REG;
        buffer[7] = 1;
        buffer[8] = 'x';
        switch (fixture) {
        case 1: return 7;                       /* truncated header */
        case 2: buffer[4] = 0; return 16;        /* zero record length */
        case 3: return 9;                       /* length exceeds read */
        case 4: buffer[7] = (char)255; return 16;/* name exceeds record */
        case 5: buffer[9] = 'x'; return 16;      /* missing NUL */
        case 6: return bytes + 1;               /* impossible read count */
        default: assert(0);
        }
    }
    char raw[64 * 1024];
    const long count = syscall(SYS_getdents64, fd, raw, sizeof(raw));
    if (count <= 0) return count;
    size_t out = 0;
    for (size_t at = 0; at < (size_t)count;) {
        uint64_t ino64;
        uint16_t length;
        memcpy(&ino64, raw + at, 8);
        memcpy(&length, raw + at + 16, 2);
        assert(length >= 20 && at + length <= (size_t)count);
        const size_t name_length = strlen(raw + at + 19);
        const uint16_t record_length = (8 + name_length + 1 + 3) & ~3;
        assert(name_length <= 255 && out + record_length <= (size_t)bytes);
        uint32_t inode = (uint32_t)ino64;
        if (!inode && ino64) inode = 1; /* host inodes may exceed 32 bits */
        memset(buffer + out, 0, record_length);
        memcpy(buffer + out, &inode, 4);
        memcpy(buffer + out + 4, &record_length, 2);
        buffer[out + 6] = raw[at + 18];
        buffer[out + 7] = (char)name_length;
        memcpy(buffer + out + 8, raw + at + 19, name_length + 1);
        out += record_length;
        at += length;
    }
    return out;
}

static void compare_cwd(void)
{
    char expected[1024], actual[1024];
    assert(getcwd(expected, sizeof(expected)));
    assert(__wrap_getcwd(actual, sizeof(actual)) == actual);
    assert(!strcmp(expected, actual));
    char *allocated = __wrap_getcwd(NULL, 0);
    assert(allocated && !strcmp(expected, allocated));
    free(allocated);
    assert(__wrap_getcwd(actual, strlen(expected) + 1));
    errno = 0;
    assert(!__wrap_getcwd(actual, strlen(expected)) && errno == ERANGE);
    errno = 0;
    assert(!__wrap_getcwd(actual, 0) && errno == EINVAL);
    errno = 0;
    assert(!__wrap_getcwd(NULL, 1) && errno == ERANGE);
}

int main(void)
{
    int saved = open(".", O_RDONLY | O_DIRECTORY);
    assert(saved >= 0);
    assert(chdir("/") == 0);
    compare_cwd();
    /* /proc is a separate mount on the test host. */
    assert(chdir("/proc") == 0);
    compare_cwd();
    char base[] = "/tmp/pemu-native-directory-XXXXXX";
    assert(mkdtemp(base) && chdir(base) == 0);
    assert(mkdir("child", 0700) == 0);
    assert(symlink("child", "alias") == 0);
    assert(chdir("alias") == 0);
    compare_cwd();
    assert(rename("../child", "../renamed") == 0);
    compare_cwd();
    assert(chdir("..") == 0);
    char longest[256];
    memset(longest, 'a', sizeof(longest) - 1); longest[255] = 0;
    int file = open(longest, O_WRONLY | O_CREAT, 0600);
    assert(file >= 0);
    errno = 0;
    assert(!__wrap_fdopendir(file) && errno == ENOTDIR);
    assert(fcntl(file, F_GETFD) >= 0); /* failed fdopendir retains ownership */
    close(file);
    assert(!__wrap_opendir(longest));
    DIR *directory = __wrap_opendir(".");
    assert(directory);
    int fd = __wrap_dirfd(directory), found = 0;
    interrupted = 1;
    struct dirent *entry;
    errno = 0;
    while ((entry = __wrap_readdir(directory)))
        found += !strcmp(entry->d_name, longest);
    assert(found == 1 && errno == 0);
    __wrap_rewinddir(directory);
    assert(__wrap_readdir(directory));
    assert(__wrap_closedir(directory) == 0);
    assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    for (fixture = 1; fixture <= 6; ++fixture) {
        directory = __wrap_opendir(".");
        assert(directory);
        errno = 0;
        assert(!__wrap_readdir(directory) && errno == EIO);
        assert(!__wrap_readdir(directory));
        __wrap_closedir(directory);
    }
    fixture = 0;
    assert(unlink(longest) == 0 && unlink("alias") == 0);
    assert(chdir("renamed") == 0 && rmdir("../renamed") == 0);
    char buffer[1024];
    errno = 0;
    assert(!__wrap_getcwd(buffer, sizeof(buffer)) && errno == ENOENT);
    assert(fchdir(saved) == 0);
    close(saved);
    assert(rmdir(base) == 0);
    puts("PASS native directory: cwd, root/mount/symlink/rename, sizes, ownership, EINTR, malformed records");
    return 0;
}
