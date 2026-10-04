// Rename without replacing, for `semu mods migrate`: the kernel refuses when the destination exists, so nothing
// that appears between a check and the rename is ever overwritten (plain rename(2) silently replaces an empty
// folder). renameat2(RENAME_NOREPLACE) on Linux, renamex_np(RENAME_EXCL) on macOS; elsewhere it refuses.
#ifndef SEMU_RENAME_H
#define SEMU_RENAME_H

#include <errno.h>
#include <stdio.h>

#if defined(__linux__)
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif
static int semu_rename_exclusive(const char *source, const char *destination) {
#if defined(SYS_renameat2)
    return (int)syscall(SYS_renameat2, AT_FDCWD, source, AT_FDCWD, destination, RENAME_NOREPLACE);
#else
    errno = ENOSYS;
    return -1;
#endif
}
#elif defined(__APPLE__)
static int semu_rename_exclusive(const char *source, const char *destination) { return renamex_np(source, destination, RENAME_EXCL); }
#else
static int semu_rename_exclusive(const char *source, const char *destination) {
    (void)source;
    (void)destination;
    errno = ENOSYS;
    return -1;
}
#endif

#endif
