#!/bin/sh
# End-to-end check of the unified input path on a private Xvfb display: a truthfully named
# virtual gamepad (tests/visual/virtual_pad.btrc over /dev/uinput) opens the Semu menu
# (Select+Y), saves a state from it, toggles the bezel live and quits with Start+Select.
# Captures land next to OUT_PREFIX. Needs Xvfb, xwd, ImageMagick, nix and /dev/uinput access.
# usage: menu-e2e.sh OUT_PREFIX   (SEMU, SEMU_ASSET_ROOT, SEMU_SOURCE_ROOT as for semu launch)
set -eu
out="$1"
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
semu="${SEMU:-semu}"
work="$(mktemp -d)"
display=":${SEMU_CAPTURE_DISPLAY:-97}"
nix run --no-warn-dirty "$root#btrcpy" -- --strict-imports --no-cache --no-stdlib "$here/virtual_pad.btrc" -o "$work/virtual_pad.c" >/dev/null
cc -std=c11 -O1 -w -I"$root/src/launch" "$work/virtual_pad.c" -o "$work/virtual_pad"
states="$("$semu" path content_root)/states"
touch "$work/started"
Xvfb "$display" -screen 0 1920x1080x24 >/dev/null 2>&1 & xvfb=$!
trap 'kill "$xvfb" 2>/dev/null || true; rm -rf "$work"' EXIT
sleep 1
DISPLAY="$display" WAYLAND_DISPLAY= SDL_VIDEODRIVER=x11 SEMU_RENDER_DEBUG=1 \
  "$semu" launch retroarch --system gb --rom "Tetris (World) (Rev 1).zip" >"$out.log" 2>&1 & launcher=$!
"$work/virtual_pad" 14 \
  hold:select press:north release:select sleep:2.5 press:dpad_down press:south sleep:3.5 \
  hold:select press:north release:select press:dpad_down press:dpad_down press:dpad_down press:dpad_down press:south sleep:3.5 \
  press:east sleep:2 hold:select hold:start release:start release:select >"$out.pad.log" 2>&1 & pad=$!
sleep 16; DISPLAY="$display" xwd -root -silent | magick xwd:- "$out.a-menu.png"
sleep 4;  DISPLAY="$display" xwd -root -silent | magick xwd:- "$out.b-saved.png"
sleep 5;  DISPLAY="$display" xwd -root -silent | magick xwd:- "$out.c-bezel-off.png"
wait "$pad"; sleep 4
if kill -0 "$launcher" 2>/dev/null; then echo "FAIL: launcher still running after Start+Select" >&2; kill -TERM "$launcher"; wait "$launcher" || true; exit 1; fi
wait "$launcher" || true
grep -q 'semu: action ui.menu (gamepad)' "$out.log" || { echo "FAIL: Select+Y did not open the menu" >&2; exit 1; }
grep -q 'semu: action ui.menu.confirm (gamepad)' "$out.log" || { echo "FAIL: menu confirm never fired" >&2; exit 1; }
[ -n "$(find "$states" -type f -newer "$work/started" 2>/dev/null | head -1)" ] || { echo "FAIL: SAVE STATE wrote no state file under $states" >&2; exit 1; }
[ "$("$semu" settings get visual.bezels)" = "false" ] || { echo "FAIL: BEZEL ON/OFF did not persist visual.bezels=false" >&2; exit 1; }
grep -q 'semu: quit' "$out.log" || { echo "FAIL: quit chord did not end the session" >&2; exit 1; }
echo "PASS: menu, state file, persisted bezel toggle and quit drove a real RetroArch session; inspect $out.*.png"
