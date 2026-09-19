#!/bin/sh
# End-to-end check of the unified input path on a private Xvfb display: a truthfully named
# virtual gamepad opens the Semu menu (Select+Y), saves a state from it, toggles the bezel
# live and quits with Start+Select. Captures land next to OUT_PREFIX. Needs Xvfb, xwd,
# ImageMagick and python-evdev (nix-shell -p python3Packages.evdev) plus /dev/uinput access.
# usage: menu-e2e.sh OUT_PREFIX   (SEMU, SEMU_ASSET_ROOT, SEMU_SOURCE_ROOT as for semu launch)
set -eu
out="$1"
here="$(cd "$(dirname "$0")" && pwd)"
display=":${SEMU_CAPTURE_DISPLAY:-97}"
Xvfb "$display" -screen 0 1920x1080x24 >/dev/null 2>&1 & xvfb=$!
trap 'kill "$xvfb" 2>/dev/null || true' EXIT
sleep 1
DISPLAY="$display" WAYLAND_DISPLAY= SDL_VIDEODRIVER=x11 SEMU_RENDER_DEBUG=1 \
  "${SEMU:-semu}" launch retroarch --system gb --rom "Tetris (World) (Rev 1).zip" >"$out.log" 2>&1 & launcher=$!
nix-shell -p python3Packages.evdev --run "python3 '$here/virtualpad.py' 14 \
  hold:select press:north release:select sleep:2.5 press:dpad_down press:south sleep:3.5 \
  hold:select press:north release:select press:dpad_down press:dpad_down press:dpad_down press:dpad_down press:south sleep:3.5 \
  press:east sleep:2 hold:select hold:start release:start release:select" >"$out.pad.log" 2>&1 & pad=$!
sleep 16; DISPLAY="$display" xwd -root -silent | convert xwd:- "$out.a-menu.png"
sleep 4;  DISPLAY="$display" xwd -root -silent | convert xwd:- "$out.b-saved.png"
sleep 5;  DISPLAY="$display" xwd -root -silent | convert xwd:- "$out.c-bezel-off.png"
wait "$pad"; sleep 4
if kill -0 "$launcher" 2>/dev/null; then echo "FAIL: launcher still running after Start+Select" >&2; kill -TERM "$launcher"; wait "$launcher" || true; exit 1; fi
wait "$launcher" || true
grep -q 'semu: action ui.menu (gamepad)' "$out.log" || { echo "FAIL: Select+Y did not open the menu" >&2; exit 1; }
grep -q 'semu: action ui.menu.confirm (gamepad)' "$out.log" || { echo "FAIL: menu confirm never fired" >&2; exit 1; }
grep -q 'semu: quit' "$out.log" || { echo "FAIL: quit chord did not end the session" >&2; exit 1; }
echo "PASS: menu, save, bezel toggle and quit drove a real RetroArch session; inspect $out.*.png"
