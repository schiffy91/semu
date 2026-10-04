// The X11 wire facts the optional key adapter needs: XInput2 raw key events through libxcb,
// loaded at run time. Only constants and byte offsets live here; no X header is required.
#ifndef SEMU_X11_H
#define SEMU_X11_H

#ifndef SEMU_XCB_LIBRARY
#define SEMU_XCB_LIBRARY "libxcb.so.1"  // Nix bakes the store path in on Linux
#endif
#ifndef SEMU_XCB_INPUT_LIBRARY
#define SEMU_XCB_INPUT_LIBRARY "libxcb-xinput.so.0"  // from the same libxcb package as above
#endif
#ifndef SEMU_XCB_TEST_LIBRARY
#define SEMU_XCB_TEST_LIBRARY "libxcb-xtest.so.0"  // XTest, to type an emulator's own hotkey on its display
#endif

#define SEMU_X11_GENERIC_EVENT 35  // XCB_GE_GENERIC: an extension event of any length
#define SEMU_X11_SEND_EVENT_BIT 0x80  // set on events that another client sent
#define SEMU_X11_RAW_KEY_PRESS 13  // XI_RawKeyPress, delivered to the root window only
#define SEMU_X11_RAW_KEY_RELEASE 14  // XI_RawKeyRelease, delivered to the root window only
#define SEMU_X11_RAW_KEY_MASK ((1u << 13) | (1u << 14))
#define SEMU_X11_ALL_MASTER_DEVICES 1  // XIAllMasterDevices: one event per key, never per slave
#define SEMU_X11_KEY_REPEAT_FLAG (1u << 16)  // XIKeyRepeat, in case a server repeats raw keys
#define SEMU_X11_KEYCODE_OFFSET 8  // an X keycode is the evdev code plus eight
#define SEMU_X11_KEY_PRESS 2  // XCB_KEY_PRESS, the type xcb_test_fake_input takes
#define SEMU_X11_KEY_RELEASE 3  // XCB_KEY_RELEASE
#define SEMU_X11_EVENT_SIZE 32  // every event's fixed part; generic events add words
#define SEMU_X11_BUFFER 512  // bytes a wire key source holds between reads

#define SEMU_X11_EVENT_EXTENSION 1  // u8: the major opcode of the sending extension
#define SEMU_X11_EVENT_LENGTH 4  // u32: extra four-byte words after the fixed part
#define SEMU_X11_EVENT_TYPE 8  // u16: the XInput2 event type of a generic event
#define SEMU_X11_EVENT_DETAIL 16  // u32: the X keycode of a raw key event
#define SEMU_X11_EVENT_FLAGS 24  // u32: the raw event flags, XIKeyRepeat among them

#define SEMU_X11_REPLY_PRESENT 8  // u8: xcb_query_extension_reply_t present
#define SEMU_X11_REPLY_OPCODE 9  // u8: xcb_query_extension_reply_t major opcode

#define SEMU_X11_SETUP_LENGTH 6  // u16: setup words that follow the first eight bytes
#define SEMU_X11_SETUP_VENDOR_LENGTH 24  // u16: the vendor string length in bytes
#define SEMU_X11_SETUP_ROOTS 28  // u8: how many screens the setup describes
#define SEMU_X11_SETUP_FORMATS 29  // u8: how many eight-byte pixmap formats follow
#define SEMU_X11_SETUP_FIXED 40  // where the padded vendor string begins
#define SEMU_X11_SCREEN_FIXED 40  // xcb_screen_t before its depths; root window comes first
#define SEMU_X11_SCREEN_DEPTHS 39  // u8: how many allowed depths the screen lists
#define SEMU_X11_DEPTH_FIXED 8  // xcb_depth_t before its 24-byte visual records
#define SEMU_X11_DEPTH_VISUALS 2  // u16: how many visuals the depth lists
#define SEMU_X11_VISUAL_SIZE 24  // one xcb_visualtype_t record in bytes

#endif
