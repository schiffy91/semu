// Evdev and uinput for the input supervisor: the kernel headers on Linux, and on every
// other host the same constants and records so it compiles, finds no /dev/input, idles.
#ifndef SEMU_EVDEV_H
#define SEMU_EVDEV_H

#if defined(__linux__)
#include <linux/input.h>
#include <linux/uinput.h>
#define SEMU_HAS_EVDEV 1
#else
#include <stdint.h>
#include <sys/time.h>
#define SEMU_HAS_EVDEV 0
struct input_event { struct timeval time; uint16_t type; uint16_t code; int32_t value; };
struct input_id { uint16_t bustype; uint16_t vendor; uint16_t product; uint16_t version; };
#define UINPUT_MAX_NAME_SIZE 80
struct uinput_setup { struct input_id id; char name[UINPUT_MAX_NAME_SIZE]; uint32_t ff_effects_max; };
#define BUS_VIRTUAL 0x06
#define EVIOCGNAME(len) 0UL
#define EVIOCGBIT(ev, len) 0UL
#define UI_DEV_CREATE 0UL
#define UI_DEV_DESTROY 0UL
#define UI_DEV_SETUP 0UL
#define UI_SET_EVBIT 0UL
#define UI_SET_KEYBIT 0UL
#define ABS_HAT0X 0x10
#define ABS_HAT0Y 0x11
#define BTN_DPAD_DOWN 0x221
#define BTN_DPAD_LEFT 0x222
#define BTN_DPAD_RIGHT 0x223
#define BTN_DPAD_UP 0x220
#define BTN_EAST 0x131
#define BTN_MODE 0x13c
#define BTN_NORTH 0x133
#define BTN_SELECT 0x13a
#define BTN_SOUTH 0x130
#define BTN_START 0x13b
#define BTN_THUMBL 0x13d
#define BTN_THUMBR 0x13e
#define BTN_TL 0x136
#define BTN_TL2 0x138
#define BTN_TR 0x137
#define BTN_TR2 0x139
#define BTN_WEST 0x134
#define EV_ABS 0x03
#define EV_KEY 0x01
#define EV_SYN 0x00
#define KEY_0 11
#define KEY_1 2
#define KEY_2 3
#define KEY_3 4
#define KEY_4 5
#define KEY_5 6
#define KEY_6 7
#define KEY_7 8
#define KEY_8 9
#define KEY_9 10
#define KEY_A 30
#define KEY_B 48
#define KEY_BACKSPACE 14
#define KEY_C 46
#define KEY_D 32
#define KEY_DOWN 108
#define KEY_E 18
#define KEY_ENTER 28
#define KEY_EQUAL 13
#define KEY_ESC 1
#define KEY_F 33
#define KEY_F1 59
#define KEY_F10 68
#define KEY_F11 87
#define KEY_F12 88
#define KEY_F2 60
#define KEY_F3 61
#define KEY_F4 62
#define KEY_F5 63
#define KEY_F6 64
#define KEY_F7 65
#define KEY_F8 66
#define KEY_F9 67
#define KEY_G 34
#define KEY_H 35
#define KEY_I 23
#define KEY_J 36
#define KEY_K 37
#define KEY_L 38
#define KEY_LEFT 105
#define KEY_LEFTALT 56
#define KEY_LEFTCTRL 29
#define KEY_LEFTMETA 125
#define KEY_LEFTSHIFT 42
#define KEY_M 50
#define KEY_MINUS 12
#define KEY_N 49
#define KEY_O 24
#define KEY_P 25
#define KEY_Q 16
#define KEY_R 19
#define KEY_RIGHT 106
#define KEY_RIGHTALT 100
#define KEY_RIGHTCTRL 97
#define KEY_RIGHTMETA 126
#define KEY_RIGHTSHIFT 54
#define KEY_S 31
#define KEY_SPACE 57
#define KEY_T 20
#define KEY_TAB 15
#define KEY_U 22
#define KEY_UP 103
#define KEY_V 47
#define KEY_W 17
#define KEY_X 45
#define KEY_Y 21
#define KEY_Z 44
#define SYN_REPORT 0
#endif

#endif
