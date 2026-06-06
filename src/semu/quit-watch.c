#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/input.h>
#endif

#ifndef KEY_ESC
#define KEY_ESC 1
#define KEY_Q 16
#define KEY_LEFTCTRL 29
#define KEY_LEFTALT 56
#define KEY_F4 62
#define KEY_RIGHTCTRL 97
#define KEY_RIGHTALT 100
#define BTN_SELECT 314
#define BTN_START 315
#define EV_KEY 1
#endif

#define SEMU_MAX_INPUT_FDS 128
#define SEMU_MAX_KEYS 768
#define SEMU_RESCAN_TICKS 2

typedef struct {
    int fd;
    char path[256];
} SemuInputFd;

typedef struct {
    SemuInputFd items[SEMU_MAX_INPUT_FDS];
    size_t len;
    bool down[SEMU_MAX_KEYS];
} SemuInputWatcher;

static bool semu_debug_enabled(void) {
    const char *value = getenv("SEMU_QUIT_WATCH_DEBUG");
    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

#define SEMU_DEBUG(...) \
    do { \
        if (semu_debug_enabled()) { \
            fprintf(stderr, "semu-quit-watch: " __VA_ARGS__); \
            fputc('\n', stderr); \
        } \
    } while (0)

static bool semu_path_is_open(SemuInputWatcher *watcher, const char *path) {
    for (size_t i = 0; i < watcher->len; i++) {
        if (strcmp(watcher->items[i].path, path) == 0) {
            return true;
        }
    }
    return false;
}

static void semu_close_input_fd(SemuInputWatcher *watcher, size_t index) {
    if (index >= watcher->len) {
        return;
    }
    SEMU_DEBUG("close %s", watcher->items[index].path);
    close(watcher->items[index].fd);
    watcher->items[index] = watcher->items[watcher->len - 1];
    watcher->len--;
}

static void semu_close_inputs(SemuInputWatcher *watcher) {
    for (size_t i = 0; i < watcher->len; i++) {
        close(watcher->items[i].fd);
    }
    watcher->len = 0;
}

static void semu_scan_inputs(SemuInputWatcher *watcher) {
#ifndef __linux__
    (void)watcher;
#else
    DIR *dir = opendir("/dev/input");
    if (dir == NULL) {
        SEMU_DEBUG("open /dev/input failed: %s", strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }
        if (watcher->len >= SEMU_MAX_INPUT_FDS) {
            break;
        }

        char path[256];
        int written = snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path) || semu_path_is_open(watcher, path)) {
            continue;
        }

        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            SEMU_DEBUG("open %s failed: %s", path, strerror(errno));
            continue;
        }

        watcher->items[watcher->len].fd = fd;
        strncpy(watcher->items[watcher->len].path, path, sizeof(watcher->items[watcher->len].path) - 1);
        watcher->items[watcher->len].path[sizeof(watcher->items[watcher->len].path) - 1] = '\0';
        watcher->len++;
        SEMU_DEBUG("watch %s", path);
    }
    closedir(dir);
#endif
}

static bool semu_key_down(SemuInputWatcher *watcher, int code) {
    return code >= 0 && code < SEMU_MAX_KEYS && watcher->down[code];
}

static bool semu_record_key(SemuInputWatcher *watcher, int code, int value) {
    if (code < 0 || code >= SEMU_MAX_KEYS) {
        return false;
    }
    if (value == 0) {
        watcher->down[code] = false;
        return false;
    }

    watcher->down[code] = true;
    bool ctrl = semu_key_down(watcher, KEY_LEFTCTRL) || semu_key_down(watcher, KEY_RIGHTCTRL);
    bool alt = semu_key_down(watcher, KEY_LEFTALT) || semu_key_down(watcher, KEY_RIGHTALT);
    bool select_start = semu_key_down(watcher, BTN_SELECT) && semu_key_down(watcher, BTN_START);

    if (code == KEY_ESC) {
        SEMU_DEBUG("quit key: escape");
        return true;
    }
    if (code == KEY_Q && ctrl) {
        SEMU_DEBUG("quit key: ctrl+q");
        return true;
    }
    if (code == KEY_F4 && alt) {
        SEMU_DEBUG("quit key: alt+f4");
        return true;
    }
    if ((code == BTN_SELECT || code == BTN_START) && select_start) {
        SEMU_DEBUG("quit key: select+start");
        return true;
    }
    return false;
}

static bool semu_poll_quit(SemuInputWatcher *watcher, int timeout_ms) {
#ifndef __linux__
    (void)watcher;
    (void)timeout_ms;
    return false;
#else
    struct pollfd polls[SEMU_MAX_INPUT_FDS];
    for (size_t i = 0; i < watcher->len; i++) {
        polls[i].fd = watcher->items[i].fd;
        polls[i].events = POLLIN;
        polls[i].revents = 0;
    }

    int ready = poll(polls, watcher->len, timeout_ms);
    if (ready <= 0) {
        return false;
    }

    for (size_t i = 0; i < watcher->len;) {
        if (polls[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            semu_close_input_fd(watcher, i);
            continue;
        }
        if (!(polls[i].revents & POLLIN)) {
            i++;
            continue;
        }

        struct input_event events[32];
        ssize_t nread = read(watcher->items[i].fd, events, sizeof(events));
        if (nread < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                semu_close_input_fd(watcher, i);
                continue;
            }
        } else {
            size_t count = (size_t)nread / sizeof(struct input_event);
            for (size_t j = 0; j < count; j++) {
                if (events[j].type == EV_KEY) {
                    SEMU_DEBUG("event %s key=%u value=%d", watcher->items[i].path, events[j].code, events[j].value);
                }
                if (events[j].type == EV_KEY && semu_record_key(watcher, events[j].code, events[j].value)) {
                    return true;
                }
            }
        }
        i++;
    }
    return false;
#endif
}

static int semu_status_code(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

static void semu_terminate_child_group(pid_t child) {
    kill(-child, SIGTERM);
    for (int i = 0; i < 20; i++) {
        int status = 0;
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) {
            return;
        }
        usleep(100000);
    }
    kill(-child, SIGKILL);
}

int main(int argc, char **argv) {
    int command_index = 1;
    if (argc > 1 && strcmp(argv[1], "--") == 0) {
        command_index = 2;
    }
    if (command_index >= argc) {
        fprintf(stderr, "usage: semu-quit-watch [--] COMMAND [ARGS...]\n");
        return 64;
    }

    const char *enabled = getenv("SEMU_QUIT_WATCH");
    if (enabled != NULL && strcmp(enabled, "0") == 0) {
        execvp(argv[command_index], &argv[command_index]);
        perror("execvp");
        return 127;
    }

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return 127;
    }
    if (child == 0) {
        setpgid(0, 0);
        execvp(argv[command_index], &argv[command_index]);
        perror("execvp");
        _exit(127);
    }
    setpgid(child, child);
    SEMU_DEBUG("child pid=%ld", (long)child);

    SemuInputWatcher watcher;
    memset(&watcher, 0, sizeof(watcher));
    int scan_ticks = 0;
    bool quit_requested = false;

    for (;;) {
        int status = 0;
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) {
            semu_close_inputs(&watcher);
            return quit_requested ? 0 : semu_status_code(status);
        }

        if (scan_ticks <= 0) {
            semu_scan_inputs(&watcher);
            scan_ticks = SEMU_RESCAN_TICKS;
        }
        scan_ticks--;

        if (semu_poll_quit(&watcher, 100)) {
            quit_requested = true;
            SEMU_DEBUG("quit requested");
            semu_terminate_child_group(child);
            semu_close_inputs(&watcher);
            return 0;
        }
    }
}
