#!/bin/sh
# Capture the composited frame of one launch on a private Xvfb display with llvmpipe,
# so the real desktop is never touched. usage: capture.sh OUT.png EMULATOR launch-args...
# Needs Xvfb, xwd and ImageMagick on PATH; SEMU names the CLI (default: semu).
set -eu
out="$1"; shift
display=":${SEMU_CAPTURE_DISPLAY:-97}"
Xvfb "$display" -screen 0 "${SEMU_CAPTURE_SIZE:-1920x1080x24}" >/dev/null 2>&1 & xvfb=$!
trap 'kill "$xvfb" 2>/dev/null || true' EXIT
sleep 1
DISPLAY="$display" WAYLAND_DISPLAY= LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 QT_QPA_PLATFORM=xcb \
  XDG_RUNTIME_DIR=/nonexistent PULSE_SERVER=/nonexistent PIPEWIRE_RUNTIME_DIR=/nonexistent SDL_AUDIODRIVER=dummy \
  "${SEMU:-semu}" launch "$@" >"${SEMU_CAPTURE_LOG:-/dev/null}" 2>&1 & pid=$!  # no audio server reachable: a headless capture stays silent
wait="${SEMU_CAPTURE_WAIT:-15}"
sleep $((wait / 2))
if command -v xdotool >/dev/null 2>&1; then  # no window manager on Xvfb: native emulators keep their default window size unless we maximize
  for window in $(DISPLAY="$display" xdotool search --onlyvisible --name '.' 2>/dev/null); do
    DISPLAY="$display" xdotool windowmove "$window" 0 0 windowsize "$window" 100% 100% 2>/dev/null || true
  done
fi
sleep $((wait - wait / 2))
xwd -root -silent -display "$display" | convert xwd:- "$out"
kill -TERM "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true
echo "$out"
