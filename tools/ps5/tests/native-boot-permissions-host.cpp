// Link native-app-crt.cpp with renamed _start/_init/_fini and --gc-sections.
// The real boot writer runs against POSIX-backed libkernel test imports.
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

extern "C" void pemu_boot_write(const char *, size_t);
extern "C" int pemu_boot_fix_permissions(int (*)(const char *, unsigned int));
static std::string directory;
static int fallback, chmod_calls;

extern "C" int sceKernelOpen(const char *path, int flags, unsigned int mode)
{
    const char *names[] = {"/app0/pemu_boot.log", "/download0/pemu_boot.log",
                           "/data/pemu_boot.log"};
    for (int i = 0; i < 3; ++i) {
        if (strcmp(path, names[i])) continue;
        if (i < fallback) { errno = ENOENT; return -1; }
        assert(i == fallback);
        return open((directory + "/" + std::to_string(i)).c_str(), flags, mode);
    }
    assert(false);
    return -1;
}
extern "C" long sceKernelWrite(int fd, const void *buffer, size_t length)
{
    // Also exercise the writer's partial-write loop.
    return write(fd, buffer, length > 3 ? 3 : length);
}
extern "C" int sceKernelClose(int fd) { return close(fd); }
static int test_chmod(const char *path, unsigned int mode)
{
    const char *names[] = {"/app0/pemu_boot.log", "/download0/pemu_boot.log",
                           "/data/pemu_boot.log"};
    for (int i = 0; i < 3; ++i) {
        if (strcmp(path, names[i])) continue;
        const int result = chmod((directory + "/" + std::to_string(i)).c_str(), mode);
        if (result == 0) ++chmod_calls;
        return result;
    }
    assert(false);
    return -1;
}

int main()
{
    char temporary[] = "/tmp/pemu-native-boot-XXXXXX";
    assert(mkdtemp(temporary));
    directory = temporary;
    const mode_t old_mask = umask(0777);
    constexpr char first[] = "BOOT_STAGE permissions\n";
    constexpr char next[] = "BOOT_STAGE append\n";
    for (fallback = 0; fallback < 3; ++fallback) {
        const std::string path = directory + "/" + std::to_string(fallback);
        pemu_boot_write(first, sizeof(first) - 1);
        struct stat status;
        // Even a fully masked file must contain the first record before any
        // optional permission function is called.
        assert(chmod_calls == fallback * 2);
        assert(stat(path.c_str(), &status) == 0 && status.st_size == sizeof(first) - 1);
        assert(pemu_boot_fix_permissions(nullptr) == -1);
        assert(pemu_boot_fix_permissions(test_chmod) == 0);
        assert(stat(path.c_str(), &status) == 0 && (status.st_mode & 0777) == 0644);
        assert(chmod(path.c_str(), 0600) == 0);
        pemu_boot_write(next, sizeof(next) - 1);
        assert(pemu_boot_fix_permissions(test_chmod) == 0);
        assert(stat(path.c_str(), &status) == 0 && (status.st_mode & 0777) == 0644);
        const int fd = open(path.c_str(), O_RDONLY);
        assert(fd >= 0);
        char contents[256] = {};
        assert(read(fd, contents, sizeof(contents)) == sizeof(first) + sizeof(next) - 2);
        assert(std::string(contents) == std::string(first) + next);
        close(fd);
        assert(unlink(path.c_str()) == 0);
    }
    assert(chmod_calls == 6);
    umask(old_mask);
    assert(rmdir(directory.c_str()) == 0);
    puts("PASS native boot: first record before permission API, new/existing logs 0644, umask 0777, three paths, append/short writes");
}
