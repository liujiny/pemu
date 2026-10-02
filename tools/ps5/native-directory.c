/*
 * PS5 native title directory compatibility.
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Adapted from PS5_PayloadSDK platform/src/directory.c, commit
 * fa69d00fe974a47a20009d32c9780c259a05a08f:
 * https://github.com/mihawk-99/PS5_PayloadSDK
 *
 * A title's libc getcwd calls __getcwd, which is only exported by
 * libkernel_sys. Native titles use libkernel and cannot resolve that call.
 * Its opendir is also refused. Use getdents for directory streams and walk
 * parents to determine the real working directory, including mount points.
 * Link only the native title with --wrap for the functions below. Do not mix
 * these opaque DIR objects with libc's directory functions.
 */
#define _GNU_SOURCE 1

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Resolve after the first boot records, rather than adding startup imports.
int pemu_native_getdents(int fd, char *buffer, int bytes);
int pemu_native_lstat(const char *path, struct stat *status);

#define PATH_BYTES 1024

struct native_directory {
    int fd;
    size_t offset, bytes;
    bool finished;
    struct dirent entry;
    /* Mounted title directories require a larger read than the root. */
    char buffer[64 * 1024];
};

DIR *__wrap_fdopendir(int fd)
{
    struct stat status;
    if (fstat(fd, &status) != 0)
        return NULL;
    if (!S_ISDIR(status.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }
    struct native_directory *stream = calloc(1, sizeof(*stream));
    if (!stream) {
        errno = ENOMEM;
        return NULL;
    }
    stream->fd = fd;
    return (DIR *)stream;
}

DIR *__wrap_opendir(const char *path)
{
    if (!path || !*path) {
        errno = ENOENT;
        return NULL;
    }
    const int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        return NULL;
    DIR *stream = __wrap_fdopendir(fd);
    if (!stream) {
        const int saved = errno;
        close(fd);
        errno = saved;
    }
    return stream;
}

/* FreeBSD console records: uint32 inode, uint16 record length, uint8 type,
 * uint8 name length, name and NUL. Validate before reading a variable field. */
struct dirent *__wrap_readdir(DIR *opaque)
{
    const int saved_errno = errno;
    struct native_directory *stream = (struct native_directory *)opaque;
    if (!stream) {
        errno = EBADF;
        return NULL;
    }
    while (!stream->finished) {
        if (stream->offset == stream->bytes) {
            int count;
            do {
                count = pemu_native_getdents(stream->fd, stream->buffer, sizeof(stream->buffer));
            } while (count < 0 && errno == EINTR);
            if (count <= 0 || (size_t)count > sizeof(stream->buffer)) {
                stream->finished = true;
                if (count > 0)
                    errno = EIO;
                else if (count == 0)
                    errno = saved_errno;
                return NULL;
            }
            stream->bytes = (size_t)count;
            stream->offset = 0;
        }
        const size_t remaining = stream->bytes - stream->offset;
        const char *record = stream->buffer + stream->offset;
        uint32_t inode = 0;
        uint16_t length = 0;
        if (remaining >= 8) {
            memcpy(&inode, record, 4);
            memcpy(&length, record + 4, 2);
        }
        const size_t name_length = remaining >= 8 ? (uint8_t)record[7] : 0;
        if (remaining < 8 || length < 8 + name_length + 1 || length > remaining ||
            name_length >= sizeof(stream->entry.d_name) || record[8 + name_length] != 0) {
            stream->finished = true;
            errno = EIO;
            return NULL;
        }
        stream->offset += length;
        if (!inode)
            continue;
        memset(&stream->entry, 0, sizeof(stream->entry));
        stream->entry.d_ino = inode;
        stream->entry.d_reclen = sizeof(stream->entry);
#ifdef __FreeBSD__
        stream->entry.d_namlen = name_length;
#endif
        stream->entry.d_type = (uint8_t)record[6];
        memcpy(stream->entry.d_name, record + 8, name_length + 1);
        errno = saved_errno;
        return &stream->entry;
    }
    return NULL;
}

void __wrap_rewinddir(DIR *opaque)
{
    struct native_directory *stream = (struct native_directory *)opaque;
    if (!stream) {
        errno = EBADF;
        return;
    }
    if (lseek(stream->fd, 0, SEEK_SET) < 0)
        return;
    stream->offset = stream->bytes = 0;
    stream->finished = false;
}

int __wrap_dirfd(DIR *opaque)
{
    struct native_directory *stream = (struct native_directory *)opaque;
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    return stream->fd;
}

int __wrap_closedir(DIR *opaque)
{
    struct native_directory *stream = (struct native_directory *)opaque;
    if (!stream) {
        errno = EBADF;
        return -1;
    }
    const int result = close(stream->fd);
    const int saved = errno;
    free(stream);
    errno = saved;
    return result;
}

char *__wrap_getcwd(char *buffer, size_t size)
{
    char path[PATH_BYTES];
    size_t start = sizeof(path) - 1;
    path[start] = '\0';
    char up[PATH_BYTES] = ".";
    struct stat here, parent;
    if (buffer && size == 0) {
        errno = EINVAL;
        return NULL;
    }
    if (stat(".", &here) != 0)
        return NULL;
    for (;;) {
        char above[PATH_BYTES];
        const size_t up_length = strlen(up);
        if (up_length + sizeof("/..") > sizeof(above)) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        memcpy(above, up, up_length);
        memcpy(above + up_length, "/..", sizeof("/.."));
        if (stat(above, &parent) != 0)
            return NULL;
        if (parent.st_dev == here.st_dev && parent.st_ino == here.st_ino)
            break;
        DIR *directory = __wrap_opendir(above);
        if (!directory)
            return NULL;
        bool found = false;
        int error = 0;
        while (!found) {
            errno = 0;
            struct dirent *entry = __wrap_readdir(directory);
            if (!entry) {
                error = errno ? errno : ENOENT;
                break;
            }
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                continue;
            char candidate[PATH_BYTES];
            const size_t above_length = up_length + 3;
            const size_t name_length = strlen(entry->d_name);
            if (above_length + 1 + name_length + 1 > sizeof(candidate))
                continue;
            memcpy(candidate, above, above_length);
            candidate[above_length] = '/';
            memcpy(candidate + above_length + 1, entry->d_name, name_length + 1);
            struct stat status;
            if (pemu_native_lstat(candidate, &status) == 0 && status.st_dev == here.st_dev &&
                status.st_ino == here.st_ino) {
                if (name_length + 1 > start) {
                    error = ENAMETOOLONG;
                    break;
                }
                start -= name_length;
                memcpy(path + start, entry->d_name, name_length);
                path[--start] = '/';
                found = true;
            }
        }
        __wrap_closedir(directory);
        if (!found) {
            errno = error;
            return NULL;
        }
        here = parent;
        memcpy(up, above, up_length + sizeof("/.."));
    }
    if (path[start] == '\0')
        path[--start] = '/';
    const size_t length = sizeof(path) - 1 - start;
    if (!buffer) {
        if (size && size <= length) {
            errno = ERANGE;
            return NULL;
        }
        buffer = malloc(size ? size : length + 1);
        if (!buffer) {
            errno = ENOMEM;
            return NULL;
        }
    } else if (size <= length) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buffer, path + start, length + 1);
    return buffer;
}
