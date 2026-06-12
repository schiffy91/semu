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
    {"b", BTN_EAST},
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

static int semu_open_uinput(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        semu_die("open /dev/uinput");
    }

    semu_ioctl(fd, UI_SET_EVBIT, EV_KEY, "UI_SET_EVBIT EV_KEY");
    for (size_t i = 0; i < sizeof(semu_keys) / sizeof(semu_keys[0]); i++) {
        semu_ioctl(fd, UI_SET_KEYBIT, semu_keys[i].code, "UI_SET_KEYBIT");
    }

    struct uinput_user_dev device;
    memset(&device, 0, sizeof(device));
    snprintf(device.name, sizeof(device.name), "semu-test-input");
    device.id.bustype = BUS_USB;
    device.id.vendor = 0x28de;
    device.id.product = 0x1205;
    device.id.version = 1;

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

static void semu_usage(void) {
    fprintf(stderr, "usage: semu-uinput-send ACTION...\n");
    fprintf(stderr, "actions: select-start esc enter start select a b x y l1 r1 up down left right key-z key-x gameplay-probe\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        semu_usage();
        return 64;
    }

    int fd = semu_open_uinput();
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "select-start") == 0) {
            semu_select_start(fd);
            continue;
        }
        if (strcmp(argv[i], "gameplay-probe") == 0) {
            semu_tap(fd, BTN_START);
            semu_tap(fd, BTN_SOUTH);
            semu_tap(fd, BTN_DPAD_RIGHT);
            semu_tap(fd, BTN_SOUTH);
            continue;
        }

        int code = 0;
        if (!semu_named_key(argv[i], &code)) {
            fprintf(stderr, "semu-uinput-send: unknown action '%s'\n", argv[i]);
            semu_usage();
            ioctl(fd, UI_DEV_DESTROY);
            close(fd);
            return 64;
        }
        semu_tap(fd, code);
    }

    if (ioctl(fd, UI_DEV_DESTROY) < 0) {
        semu_die("UI_DEV_DESTROY");
    }
    close(fd);
    return 0;
}
