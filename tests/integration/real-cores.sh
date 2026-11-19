#!/bin/sh
# One real core with a real (freely licensed) test program through `semu launch`: it boots, draws a frame
# that is not blank, saves a state file, loads it back, and quits through RetroArch's QUIT. No step waits
# a fixed time: each waits for the thing itself, so a busy machine is slower and not wrong, and a core
# that dies is reported as having died, with how.
# Inputs: SEMU_CLI, RETROARCH, RENDERER (renderer lib dir), SYSTEM, CORE, ROM, COREFILE.
set -eu
work="${TMPDIR:-/tmp}/semu-real-core"
command_port() { printf '%s' "$1" | socat -t 1 - UDP:127.0.0.1:55355 2>/dev/null | tr -d '\n'; }
fail() {
  echo "$SYSTEM ($CORE): $1"
  tail -5 "$work/launch.log" 2>/dev/null || true
  kill -TERM "$launcher" 2>/dev/null || true
  exit 1
}
gone() {  # RetroArch is no longer there: say how it left
  code=0
  wait "$launcher" || code=$?
  fail "RetroArch left with status $code$([ "$code" -gt 128 ] && echo " (signal $((code - 128)))") while waiting for $1"
}
# await SECONDS WHAT COMMAND...: the command is tried four times a second until it holds.
await() {
  tries=$(( $1 * 4 )); what=$2; shift 2
  while ! "$@"; do
    kill -0 "$launcher" 2>/dev/null || gone "$what"
    tries=$((tries - 1))
    [ "$tries" -gt 0 ] || fail "no $what in time"
    sleep 0.25
  done
}
answers() { [ -n "$(command_port VERSION || true)" ]; }
drawn() {  # a screenshot taken now is not one flat colour
  rm -f "$work/content/screenshots/"*.png
  command_port SCREENSHOT > /dev/null || true
  shot=""
  for _ in 1 2 3 4 5 6 7 8; do
    shot="$(find "$work/content/screenshots" -name '*.png' 2>/dev/null | head -1)"
    [ -z "$shot" ] || break
    sleep 0.25
  done
  [ -n "$shot" ] || return 1
  spread="$(magick identify -format '%[fx:standard_deviation]' "$shot" 2>/dev/null || echo 0)"
  awk "BEGIN { exit !($spread > 0.02) }"
}
saved() {  # asked again each second: ppsspp writes no state for a request that comes too early
  state="$(find "$work/content/states" -name '*.state*' 2>/dev/null | head -1)"
  [ -z "$state" ] || return 0
  asked=$((asked + 1))
  [ $((asked % 4)) -ne 1 ] || command_port SAVE_STATE > /dev/null || true
  return 1
}

config="$(dirname "$(readlink -f "$SEMU_CLI")")/../share/semu/config"
rm -rf "$work"
mkdir -p "$work/home" "$work/assets/bin" "$work/assets/lib/retroarch/cores"
export HOME="$work/home" LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
unset WAYLAND_DISPLAY
ln -s "$RETROARCH" "$work/assets/bin/retroarch"
ln -s "$RENDERER/libsemurenderer.so" "$work/assets/lib/libsemurenderer.so"
ln -s "$COREFILE" "$work/assets/lib/retroarch/cores/${CORE}_libretro.so"
settings="{\"paths\":{\"roms\":\"$work/roms\",\"state_root\":\"$work/state\",\"content_root\":\"$work/content\"}}"
romDirectory="$work/roms/$(jq -r '.rom.dir' "$config/systems/$SYSTEM/system.json")"
mkdir -p "$romDirectory"
cp "$ROM" "$romDirectory/"
name="$(basename "$ROM")"
"$SEMU_CLI" launch retroarch --system "$SYSTEM" --rom "$name" --asset-root "$work/assets" --settings-json "$settings" > "$work/launch.log" 2>&1 &
launcher=$!
asked=0
await 90 "an answer from RetroArch" answers
await 90 "a frame that is not blank" drawn
await 45 "a state file" saved
command_port LOAD_STATE > /dev/null || true
await 45 "an answer after loading the state" answers
kill -TERM "$launcher" 2>/dev/null || true
wait "$launcher" || true
grep -q 'honoured QUIT' "$work/launch.log" || fail "did not honour QUIT"
echo "$SYSTEM ($CORE): booted $name, frame spread $spread, state $(basename "$state"), quit cleanly"
