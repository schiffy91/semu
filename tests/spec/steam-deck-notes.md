# Steam Deck harness facts kept from the old tree

## gamescope screenshot (remote_system.btrc:500-528)
```
    class string screenshot(DeckRemoteSession session, string path) {
        if (session.esDe < 2 || !DeckRemoteProcess.alive(session.esDe)
                || FileSystem.exists(path)) { return ""; }
        ExecResult result = ChildProcess.run("/usr/bin/gamescopectl",
            ["screenshot", path, "3"], "",
            DeckRemoteProcess.sessionEnvironment(session.esDe),
            CommandEnvironment.empty(), 30000,
            65536, 65536);
        if (!result.ok()) { return ""; }

        long long previousSize = -1LL;
        int stableSamples = 0;
        for (int attempt = 0; attempt < 300; attempt++) {
            long long currentSize = FileSystem.isSymlink(path)
                ? -1LL : DeckRemoteFile.size(path);
            stableSamples = currentSize > 8LL && currentSize == previousSize
                ? stableSamples + 1 : 0;
            if (stableSamples >= 2) {
                string digest = DeckRemoteHash.file(path);
                if (!digest.isEmpty() && DeckRemoteClock.sleep(100)
                        && DeckRemoteFile.size(path) == currentSize
                        && DeckRemoteHash.file(path).equals(digest)) {
                    return digest;
                }
            }
            previousSize = currentSize;
            if (!DeckRemoteClock.sleep(100)) { return ""; }
        }
        return "";
```

## uinput device construction (uinput_driver.btrc:110-166)
```
            BTN_SELECT, BTN_START, BTN_DPAD_LEFT, BTN_DPAD_RIGHT,
            BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2};
        for (size_t index = 0; index < sizeof(keys) / sizeof(int); index++) {
            if (!DeckSyntheticDeviceIo.setKey(device->descriptor, keys[index])) {
                return false;
            }
        }
        struct uinput_abs_setup horizontal;
        memset(&horizontal, 0, sizeof(horizontal));
        horizontal.code = ABS_X;
        horizontal.absinfo.minimum = -32768;
        horizontal.absinfo.maximum = 32767;
        struct uinput_abs_setup vertical = horizontal;
        vertical.code = ABS_Y;
        if (ioctl(device->descriptor, UI_ABS_SETUP, &horizontal) != 0
                || ioctl(device->descriptor, UI_ABS_SETUP, &vertical) != 0) {
            return false;
        }
        struct uinput_setup setup;
        memset(&setup, 0, sizeof(setup));
        snprintf(setup.name, UINPUT_MAX_NAME_SIZE,
            "%s", "Semu Synthetic Steam Gamepad");
        setup.id.bustype = BUS_USB;
        setup.id.vendor = 0x28de;
        setup.id.product = 0x11ff;
        setup.id.version = 1;
        return ioctl(device->descriptor, UI_DEV_SETUP, &setup) == 0
            && ioctl(device->descriptor, UI_DEV_CREATE) == 0;
    }

    class bool createCommandFacet(struct DeckSyntheticDevice* device) {
        device->descriptor = open("/dev/uinput",
            (int)O_WRONLY | (int)O_NONBLOCK | (int)O_CLOEXEC);
        if (device->descriptor < 0
                || ioctl(device->descriptor, UI_SET_EVBIT, EV_KEY) != 0) {
            return false;
        }
        int keys[] = {KEY_LEFTCTRL, KEY_LEFTALT, KEY_LEFTSHIFT, KEY_LEFTMETA,
            KEY_A, KEY_BACKSPACE, KEY_DOWN, KEY_ENTER, KEY_ESC, KEY_J, KEY_K,
            KEY_M, KEY_O, KEY_P, KEY_Q, KEY_LEFT, KEY_RIGHT, KEY_S, KEY_SPACE,
            KEY_TAB, KEY_UP, KEY_X, KEY_MINUS, KEY_KPPLUS, KEY_KPDOT, KEY_KP0,
            KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KP7,
            KEY_KP8,
            KEY_KP9, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_F13, KEY_F14,
            KEY_F15, KEY_F16};
        for (size_t index = 0; index < sizeof(keys) / sizeof(int); index++) {
            if (!DeckSyntheticDeviceIo.setKey(device->descriptor, keys[index])) {
                return false;
            }
        }
        struct uinput_setup setup;
        memset(&setup, 0, sizeof(setup));
        snprintf(setup.name, UINPUT_MAX_NAME_SIZE,
            "%s", "Semu Synthetic Command Facet");
        setup.id.bustype = BUS_USB;
        setup.id.vendor = 0x28de;
        setup.id.product = 0x1205;
```
