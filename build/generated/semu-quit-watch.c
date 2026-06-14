#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <linux/input.h>

static inline int __btrc_div_int(int a, int b) {
    if (b == 0) { fprintf(stderr, "Division by zero\n"); exit(1); }
    return a / b;
}

#define SEMU_MAX_INPUT_FDS 128

#define SEMU_MAX_KEYS 768

#define SEMU_RESCAN_TICKS 2

#define SEMU_START_SELECT_WINDOW_MS 700

#define SEMU_TERM_GRACE_MS 250

#define SEMU_KILL_GRACE_MS 1000

#define SEMU_WAIT_STEP_US 10000

#define SEMU_DEBUG(...) \
    do { \
        if (semu_debug_enabled()) { \
            fprintf(stderr, "semu-quit-watch: " __VA_ARGS__); \
            fputc('\n', stderr); \
        } \
    } while (0)

typedef struct SemuInputFd SemuInputFd;
typedef struct SemuInputWatcher SemuInputWatcher;
bool semu_debug_enabled(void);
const char* semu_evidence_path(void);
const char* semu_input_dir(void);
void semu_evidence(const char* message);
void semu_evidence_reason(const char* event, const char* reason);
void semu_set_quit_reason(const char* reason);
const char* semu_quit_reason(void);
long semu_elapsed_ms(struct timespec newer, struct timespec older);
bool semu_start_select_recent(int code);
bool semu_path_is_open(struct SemuInputWatcher* watcher, const char* path);
void semu_close_input_fd(struct SemuInputWatcher* watcher, size_t index);
void semu_close_inputs(struct SemuInputWatcher* watcher);
void semu_scan_inputs(struct SemuInputWatcher* watcher);
bool semu_key_down(struct SemuInputFd* input, int code);
bool semu_record_key(struct SemuInputFd* input, int code, int value);
bool semu_poll_quit(struct SemuInputWatcher* watcher, int timeout_ms);
int semu_status_code(int status);
bool semu_process_group_exists(pid_t child);
bool semu_wait_child_group(pid_t child, int timeout_ms);
bool semu_signal_child(pid_t child, int signal);
void semu_terminate_child(pid_t child);

struct SemuInputFd {
    int fd;
    char path[256];
    bool down[SEMU_MAX_KEYS];
};

struct SemuInputWatcher {
    struct SemuInputFd items[SEMU_MAX_INPUT_FDS];
    size_t len;
};

static const char* semu_last_quit_reason = NULL;

static struct timespec semu_last_select;

static struct timespec semu_last_start;

bool semu_debug_enabled(void) {
    const char* value = getenv("SEMU_QUIT_WATCH_DEBUG");
    return (((value != NULL) && (value[0] != '\0')) && (strcmp(value, "0") != 0));
}

const char* semu_evidence_path(void) {
    const char* value = getenv("SEMU_QUIT_WATCH_LOG");
    return (((value == NULL) || (value[0] == '\0')) ? NULL : value);
}

const char* semu_input_dir(void) {
    const char* value = getenv("SEMU_QUIT_WATCH_INPUT_DIR");
    return (((value == NULL) || (value[0] == '\0')) ? "/dev/input" : value);
}

void semu_evidence(const char* message) {
    const char* path = semu_evidence_path();
    if (path == NULL) {
        return;
    }
    FILE* file = fopen(path, "a");
    if (file == NULL) {
        return;
    }
    fprintf(file, "time=%ld ", ((long)time(NULL)));
    fprintf(file, "%s", message);
    fputc('\n', file);
    fclose(file);
}

void semu_evidence_reason(const char* event, const char* reason) {
    const char* path = semu_evidence_path();
    if (path == NULL) {
        return;
    }
    FILE* file = fopen(path, "a");
    if (file == NULL) {
        return;
    }
    fprintf(file, "time=%ld %s reason=%s", ((long)time(NULL)), event, reason);
    fputc('\n', file);
    fclose(file);
}

void semu_set_quit_reason(const char* reason) {
    (semu_last_quit_reason = reason);
}

const char* semu_quit_reason(void) {
    return ((semu_last_quit_reason == NULL) ? "unknown" : semu_last_quit_reason);
}

long semu_elapsed_ms(struct timespec newer, struct timespec older) {
    if ((older.tv_sec == 0) && (older.tv_nsec == 0)) {
        return 1000000;
    }
    long sec = ((long)(newer.tv_sec - older.tv_sec));
    long nsec = ((long)(newer.tv_nsec - older.tv_nsec));
    return ((sec * 1000) + __btrc_div_int(nsec, 1000000));
}

bool semu_start_select_recent(int code) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, (&now));
    if (code == BTN_SELECT) {
        (semu_last_select = now);
    } else if (code == BTN_START) {
        (semu_last_start = now);
    }
    long select_age = semu_elapsed_ms(now, semu_last_select);
    long start_age = semu_elapsed_ms(now, semu_last_start);
    return ((select_age <= SEMU_START_SELECT_WINDOW_MS) && (start_age <= SEMU_START_SELECT_WINDOW_MS));
}

bool semu_path_is_open(struct SemuInputWatcher* watcher, const char* path) {
    for (size_t i = 0; (i < watcher->len); (i++)) {
        if (strcmp(watcher->items[i].path, path) == 0) {
            return true;
        }
    }
    return false;
}

void semu_close_input_fd(struct SemuInputWatcher* watcher, size_t index) {
    if (index >= watcher->len) {
        return;
    }
    close(watcher->items[index].fd);
    (watcher->items[index] = watcher->items[(watcher->len - 1)]);
    (watcher->len--);
}

void semu_close_inputs(struct SemuInputWatcher* watcher) {
    for (size_t i = 0; (i < watcher->len); (i++)) {
        close(watcher->items[i].fd);
    }
    (watcher->len = 0);
}

void semu_scan_inputs(struct SemuInputWatcher* watcher) {
    const char* input_dir = semu_input_dir();
    DIR* dir = opendir(input_dir);
    if (dir == NULL) {
        SEMU_DEBUG("open %s failed: %s", input_dir, strerror(errno));
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if ((strncmp(entry->d_name, "event", 5) != 0) || (watcher->len >= SEMU_MAX_INPUT_FDS)) {
            continue;
        }
        char path[256];
        int written = snprintf(path, sizeof(path), "%s/%s", input_dir, entry->d_name);
        if (((written < 0) || (((size_t)written) >= sizeof(path))) || semu_path_is_open(watcher, path)) {
            continue;
        }
        int fd = open(path, ((O_RDONLY | O_NONBLOCK) | O_CLOEXEC));
        if (fd < 0) {
            continue;
        }
        memset((&watcher->items[watcher->len]), 0, sizeof(watcher->items[watcher->len]));
        (watcher->items[watcher->len].fd = fd);
        strncpy(watcher->items[watcher->len].path, path, (sizeof(watcher->items[watcher->len].path) - 1));
        (watcher->len++);
        semu_evidence("watch");
    }
    closedir(dir);
}

bool semu_key_down(struct SemuInputFd* input, int code) {
    return (((code >= 0) && (code < SEMU_MAX_KEYS)) && input->down[code]);
}

bool semu_record_key(struct SemuInputFd* input, int code, int value) {
    if ((code < 0) || (code >= SEMU_MAX_KEYS)) {
        return false;
    }
    if (value == 0) {
        (input->down[code] = false);
        return false;
    }
    (input->down[code] = true);
    bool ctrl = (semu_key_down(input, KEY_LEFTCTRL) || semu_key_down(input, KEY_RIGHTCTRL));
    bool alt = (semu_key_down(input, KEY_LEFTALT) || semu_key_down(input, KEY_RIGHTALT));
    bool select_start_down = (semu_key_down(input, BTN_SELECT) && semu_key_down(input, BTN_START));
    bool select_start_recent = (((code == BTN_SELECT) || (code == BTN_START)) && semu_start_select_recent(code));
    if (code == KEY_ESC) {
        semu_set_quit_reason("escape");
        return true;
    }
    if ((code == KEY_Q) && ctrl) {
        semu_set_quit_reason("ctrl+q");
        return true;
    }
    if ((code == KEY_F4) && alt) {
        semu_set_quit_reason("alt+f4");
        return true;
    }
    if (((code == BTN_SELECT) || (code == BTN_START)) && (select_start_down || select_start_recent)) {
        semu_set_quit_reason("select+start");
        return true;
    }
    return false;
}

bool semu_poll_quit(struct SemuInputWatcher* watcher, int timeout_ms) {
    struct pollfd polls[SEMU_MAX_INPUT_FDS];
    for (size_t i = 0; (i < watcher->len); (i++)) {
        (polls[i].fd = watcher->items[i].fd);
        (polls[i].events = POLLIN);
        (polls[i].revents = 0);
    }
    int ready = poll(polls, watcher->len, timeout_ms);
    if (ready <= 0) {
        return false;
    }
    for (size_t i = 0; (i < watcher->len); ) {
        if (polls[i].revents & ((POLLERR | POLLHUP) | POLLNVAL)) {
            semu_close_input_fd(watcher, i);
            continue;
        }
        if (!(polls[i].revents & POLLIN)) {
            (i++);
            continue;
        }
        struct input_event events[32];
        ssize_t nread = read(watcher->items[i].fd, events, sizeof(events));
        if (nread < 0) {
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                semu_close_input_fd(watcher, i);
                continue;
            }
        } else {
            size_t count = (((size_t)nread) / sizeof(struct input_event));
            for (size_t j = 0; (j < count); (j++)) {
                if (events[j].type == EV_KEY) {
                    semu_evidence("event");
                }
                if ((events[j].type == EV_KEY) && semu_record_key((&watcher->items[i]), events[j].code, events[j].value)) {
                    semu_evidence_reason("quit", semu_quit_reason());
                    return true;
                }
            }
        }
        (i++);
    }
    return false;
}

int semu_status_code(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return (128 + WTERMSIG(status));
    }
    return 1;
}

bool semu_process_group_exists(pid_t child) {
    if (kill((-child), 0) == 0) {
        return true;
    }
    return (errno == EPERM);
}

bool semu_wait_child_group(pid_t child, int timeout_ms) {
    int waited_ms = 0;
    bool child_done = false;
    for (; 1; ) {
        if (!child_done) {
            int status = 0;
            pid_t waited = waitpid(child, (&status), WNOHANG);
            if (waited == child) {
                (child_done = true);
            } else if ((waited < 0) && (errno == ECHILD)) {
                (child_done = true);
            }
        }
        if (child_done && (!semu_process_group_exists(child))) {
            return true;
        }
        if (waited_ms >= timeout_ms) {
            return false;
        }
        usleep(SEMU_WAIT_STEP_US);
        (waited_ms += (SEMU_WAIT_STEP_US / 1000));
    }
    return false;
}

bool semu_signal_child(pid_t child, int signal) {
    if (kill((-child), signal) == 0) {
        return true;
    }
    if (errno != ESRCH) {
        SEMU_DEBUG("signal process group %d failed: %s", ((int)child), strerror(errno));
    }
    if (kill(child, signal) == 0) {
        return true;
    }
    if (errno != ESRCH) {
        SEMU_DEBUG("signal child %d failed: %s", ((int)child), strerror(errno));
    }
    return false;
}

void semu_terminate_child(pid_t child) {
    semu_signal_child(child, SIGTERM);
    if (semu_wait_child_group(child, SEMU_TERM_GRACE_MS)) {
        return;
    }
    semu_signal_child(child, SIGKILL);
    semu_wait_child_group(child, SEMU_KILL_GRACE_MS);
}

int main(int argc, char** argv) {
    int command_index = 1;
    if ((argc > 1) && (strcmp(argv[1], "--") == 0)) {
        (command_index = 2);
    }
    if (command_index >= argc) {
        fprintf(stderr, "usage: semu-quit-watch [--] COMMAND [ARGS...]\n");
        return 64;
    }
    const char* enabled = getenv("SEMU_QUIT_WATCH");
    if ((enabled != NULL) && (strcmp(enabled, "0") == 0)) {
        execvp(argv[command_index], (&argv[command_index]));
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
        execvp(argv[command_index], (&argv[command_index]));
        perror("execvp");
        _exit(127);
    }
    setpgid(child, child);
    semu_evidence("start");
    struct SemuInputWatcher watcher;
    memset((&watcher), 0, sizeof(watcher));
    int scan_ticks = 0;
    bool quit_requested = false;
    for (; 1; ) {
        int status = 0;
        pid_t waited = waitpid(child, (&status), WNOHANG);
        if (waited == child) {
            semu_evidence("exit");
            semu_close_inputs((&watcher));
            return (quit_requested ? 0 : semu_status_code(status));
        }
        if (scan_ticks <= 0) {
            semu_scan_inputs((&watcher));
            (scan_ticks = SEMU_RESCAN_TICKS);
        }
        (scan_ticks--);
        if (semu_poll_quit((&watcher), 100)) {
            (quit_requested = true);
            fprintf(stderr, "semu-quit-watch: quit requested\n");
            semu_evidence_reason("terminate", semu_quit_reason());
            semu_terminate_child(child);
            semu_evidence("exit");
            semu_close_inputs((&watcher));
            return 0;
        }
    }
    return 1;
}
