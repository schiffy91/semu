# A truthfully named virtual gamepad for headless input tests: python-evdev over /dev/uinput.
# usage: virtualpad.py <seconds-before-first> token... ; tokens: press:<name>, hold:<name>, release:<name>, sleep:<s>
import sys, time
from evdev import UInput, ecodes as e, AbsInfo
names = {"south": e.BTN_SOUTH, "east": e.BTN_EAST, "north": e.BTN_NORTH, "west": e.BTN_WEST, "tl": e.BTN_TL, "tr": e.BTN_TR,
         "select": e.BTN_SELECT, "start": e.BTN_START, "dpad_up": e.BTN_DPAD_UP, "dpad_down": e.BTN_DPAD_DOWN,
         "dpad_left": e.BTN_DPAD_LEFT, "dpad_right": e.BTN_DPAD_RIGHT}
caps = {e.EV_KEY: list(names.values()), e.EV_ABS: [(e.ABS_X, AbsInfo(0, -32768, 32767, 16, 128, 0)), (e.ABS_Y, AbsInfo(0, -32768, 32767, 16, 128, 0))]}
ui = UInput(caps, name="Semu Test Gamepad", vendor=0x5345, product=0x4d55, bustype=e.BUS_VIRTUAL)
time.sleep(float(sys.argv[1]))
for token in sys.argv[2:]:
    kind, _, name = token.partition(":")
    if kind == "sleep": time.sleep(float(name)); continue
    code = names[name]
    if kind in ("press", "hold"): ui.write(e.EV_KEY, code, 1); ui.syn(); time.sleep(0.06)
    if kind in ("press", "release"): ui.write(e.EV_KEY, code, 0); ui.syn(); time.sleep(0.12)
time.sleep(0.3)
ui.close()
