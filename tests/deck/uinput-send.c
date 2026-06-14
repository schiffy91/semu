#define _DEFAULT_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct {
    const char *name;
    int code;
} SemuKey;

static const SemuKey semu_keys[] = {
    {"a", BTN_SOUTH},
    {"steam-a", BTN_SOUTH},
    {"b", BTN_EAST},
    {"steam-b", BTN_EAST},
    {"x", BTN_WEST},
    {"y", BTN_NORTH},
    {"l1", BTN_TL},
    {"r1", BTN_TR},
    {"select", BTN_SELECT},
    {"start", BTN_START},
    {"up", BTN_DPAD_UP},
    {"down", BTN_DPAD_DOWN},
    {"left", BTN_DPAD_LEFT},
    {"right", BTN_DPAD_RIGHT},
    {"dpad-up", BTN_DPAD_UP},
    {"dpad-down", BTN_DPAD_DOWN},
    {"dpad-left", BTN_DPAD_LEFT},
    {"dpad-right", BTN_DPAD_RIGHT},
    {"esc", KEY_ESC},
    {"enter", KEY_ENTER},
    {"space", KEY_SPACE},
    {"key-z", KEY_Z},
    {"key-x", KEY_X},
    {"key-up", KEY_UP},
    {"key-down", KEY_DOWN},
    {"key-left", KEY_LEFT},
    {"key-right", KEY_RIGHT},
    {"key-a", KEY_A},
    {"key-s", KEY_S},
    {"mouse-click", BTN_LEFT},
};

static void semu_die(const char *message) {
    fprintf(stderr, "semu-uinput-send: %s: %s\n", message, strerror(errno));
    exit(1);
}

static int semu_ioctl(int fd, unsigned long request, int value, const char *label) {
    if (ioctl(fd, request, value) < 0) {
        semu_die(label);
    }
    return 0;
}

static void semu_enable_axis(int fd, int axis) {
    semu_ioctl(fd, UI_SET_ABSBIT, axis, "UI_SET_ABSBIT");
}

static int semu_open_uinput(bool steam_identity, bool full_controller) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        semu_die("open /dev/uinput");
    }

    semu_ioctl(fd, UI_SET_EVBIT, EV_KEY, "UI_SET_EVBIT EV_KEY");
    semu_ioctl(fd, UI_SET_EVBIT, EV_REL, "UI_SET_EVBIT EV_REL");
    semu_ioctl(fd, UI_SET_RELBIT, REL_X, "UI_SET_RELBIT REL_X");
    semu_ioctl(fd, UI_SET_RELBIT, REL_Y, "UI_SET_RELBIT REL_Y");
    if (full_controller) {
        semu_ioctl(fd, UI_SET_EVBIT, EV_ABS, "UI_SET_EVBIT EV_ABS");
        semu_enable_axis(fd, ABS_X);
        semu_enable_axis(fd, ABS_Y);
        semu_enable_axis(fd, ABS_RX);
        semu_enable_axis(fd, ABS_RY);
        semu_enable_axis(fd, ABS_Z);
        semu_enable_axis(fd, ABS_RZ);
        semu_enable_axis(fd, ABS_HAT0X);
        semu_enable_axis(fd, ABS_HAT0Y);
    }
    for (size_t i = 0; i < sizeof(semu_keys) / sizeof(semu_keys[0]); i++) {
        semu_ioctl(fd, UI_SET_KEYBIT, semu_keys[i].code, "UI_SET_KEYBIT");
    }

    struct uinput_user_dev device;
    memset(&device, 0, sizeof(device));
    snprintf(device.name, sizeof(device.name), "%s", full_controller ? "Steam Deck" : (steam_identity ? "Steam Virtual Gamepad" : "semu-test-input"));
    device.id.bustype = BUS_USB;
    device.id.vendor = steam_identity ? 0x28de : 0x1209;
    device.id.product = full_controller ? 0x1205 : (steam_identity ? 0x11ff : 0x5e01);
    device.id.version = full_controller ? 0x0368 : 1;
    if (full_controller) {
        int centered_axes[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ };
        for (size_t i = 0; i < sizeof(centered_axes) / sizeof(centered_axes[0]); i++) {
            int axis = centered_axes[i];
            device.absmin[axis] = -32768;
            device.absmax[axis] = 32767;
            device.absfuzz[axis] = 16;
            device.absflat[axis] = 128;
        }
        device.absmin[ABS_HAT0X] = -1;
        device.absmax[ABS_HAT0X] = 1;
        device.absmin[ABS_HAT0Y] = -1;
        device.absmax[ABS_HAT0Y] = 1;
    }

    if (write(fd, &device, sizeof(device)) != sizeof(device)) {
        semu_die("write uinput_user_dev");
    }
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        semu_die("UI_DEV_CREATE");
    }
    usleep(250000);
    return fd;
}

static void semu_emit(int fd, int type, int code, int value) {
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    if (write(fd, &event, sizeof(event)) != sizeof(event)) {
        semu_die("write input_event");
    }
}

static void semu_sync(int fd) {
    semu_emit(fd, EV_SYN, SYN_REPORT, 0);
}

static void semu_key(int fd, int code, int value) {
    semu_emit(fd, EV_KEY, code, value);
}

static void semu_tap(int fd, int code) {
    semu_key(fd, code, 1);
    semu_sync(fd);
    usleep(90000);
    semu_key(fd, code, 0);
    semu_sync(fd);
    usleep(90000);
}

static void semu_select_start(int fd) {
    semu_key(fd, BTN_SELECT, 1);
    semu_key(fd, BTN_START, 1);
    semu_sync(fd);
    usleep(160000);
    semu_key(fd, BTN_START, 0);
    semu_key(fd, BTN_SELECT, 0);
    semu_sync(fd);
    usleep(90000);
}

static bool semu_named_key(const char *name, int *code) {
    for (size_t i = 0; i < sizeof(semu_keys) / sizeof(semu_keys[0]); i++) {
        if (strcmp(name, semu_keys[i].name) == 0) {
            *code = semu_keys[i].code;
            return true;
        }
    }
    return false;
}

static int semu_run_action(int fd, const char *action) {
    if (strcmp(action, "select-start") == 0) {
        semu_select_start(fd);
        return 0;
    }
    if (strcmp(action, "gameplay-probe") == 0) {
        semu_tap(fd, KEY_ENTER);
        usleep(250000);
        semu_tap(fd, KEY_Z);
        semu_tap(fd, KEY_RIGHT);
        semu_tap(fd, KEY_Z);
        return 0;
    }

    int code = 0;
    if (!semu_named_key(action, &code)) {
        fprintf(stderr, "semu-uinput-send: unknown action '%s'\n", action);
        return 64;
    }
    semu_tap(fd, code);
    return 0;
}

static void semu_usage(void) {
    fprintf(stderr, "usage: semu-uinput-send [--delay-ms N] [--hold-ms N] [--fifo PATH] ACTION...\n");
    fprintf(stderr, "actions: select-start esc enter start select a steam-a b steam-b x y l1 r1 up down left right key-z key-x key-up key-down key-left key-right key-a key-s mouse-click gameplay-probe\n");
}

static bool semu_uses_steam_identity(int argc, char **argv, int first_action) {
    for (int i = first_action; i < argc; i++) {
        if (strcmp(argv[i], "select-start") == 0 || strcmp(argv[i], "start") == 0 || strcmp(argv[i], "select") == 0 || strncmp(argv[i], "steam-", 6) == 0) {
            return true;
        }
    }
    return false;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        semu_usage();
        return 64;
    }

    int delay_ms = 0;
    int hold_ms = 0;
    const char *fifo_path = NULL;
    int first_action = 1;
    while (first_action < argc) {
        if (strcmp(argv[first_action], "--delay-ms") == 0 && first_action + 1 < argc) {
            delay_ms = atoi(argv[first_action + 1]);
            first_action += 2;
            continue;
        }
        if (strcmp(argv[first_action], "--hold-ms") == 0 && first_action + 1 < argc) {
            hold_ms = atoi(argv[first_action + 1]);
            first_action += 2;
            continue;
        }
        if (strcmp(argv[first_action], "--fifo") == 0 && first_action + 1 < argc) {
            fifo_path = argv[first_action + 1];
            first_action += 2;
            continue;
        }
        break;
    }
    if (first_action >= argc && fifo_path == NULL) {
        semu_usage();
        return 64;
    }

    int fd = semu_open_uinput(fifo_path != NULL || semu_uses_steam_identity(argc, argv, first_action), fifo_path != NULL);
    if (delay_ms > 0) {
        usleep((useconds_t)delay_ms * 1000);
    }

    if (fifo_path != NULL) {
        int fifo_fd = open(fifo_path, O_RDWR | O_CLOEXEC);
        if (fifo_fd < 0) {
            semu_die("open fifo");
        }
        FILE *fifo = fdopen(fifo_fd, "r");
        if (fifo == NULL) {
            semu_die("fdopen fifo");
        }
        char line[128];
        while (fgets(line, sizeof(line), fifo) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] == '\0') {
                continue;
            }
            if (strcmp(line, "quit") == 0) {
                break;
            }
            int action_status = semu_run_action(fd, line);
            if (action_status != 0) {
                fclose(fifo);
                ioctl(fd, UI_DEV_DESTROY);
                close(fd);
                return action_status;
            }
        }
        fclose(fifo);
    }

    for (int i = first_action; i < argc; i++) {
        int action_status = semu_run_action(fd, argv[i]);
        if (action_status != 0) {
            semu_usage();
            ioctl(fd, UI_DEV_DESTROY);
            close(fd);
            return action_status;
        }
    }
    if (hold_ms > 0) {
        usleep((useconds_t)hold_ms * 1000);
    }

    if (ioctl(fd, UI_DEV_DESTROY) < 0) {
        semu_die("UI_DEV_DESTROY");
    }
    close(fd);
    return 0;
}
