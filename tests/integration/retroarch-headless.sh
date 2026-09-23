#!/bin/sh
# Runs real RetroArch under Xvfb with the generated linux-desktop profile and a
# content-free core: it must reach PLAYING, answer network commands, take a
# non-blank screenshot, and exit on QUIT. Inputs: SEMU_CLI, RETROARCH, CORE.
set -eu
work="${TMPDIR:-/tmp}/semu-headless"
command_port() {  # send one RetroArch network command and print the reply, if any
  printf '%s' "$1" | socat -t 1 - UDP:127.0.0.1:55355 2>/dev/null | tr -d '\n'
}
rm -rf "$work"
mkdir -p "$work/home" "$work/assets/bin" "$work/roms/gb" "$work/screenshots" "$work/content/saves" "$work/content/states" "$work/content/screenshots"
export HOME="$work/home"
printf 'semu synthetic content\n' > "$work/roms/gb/pattern.semu"

"$SEMU_CLI" build configs --target linux-desktop --asset-root "$work/assets" \
  --settings-json "{\"paths\":{\"roms\":\"$work/roms\",\"state_root\":\"$work/state\",\"content_root\":\"$work/content\"}}" \
  --output "$work/out" > "$work/configs.log"
config="$work/out/profiles/retroarch/retroarch.cfg"
grep -q 'video_fullscreen = "true"' "$config"

cat > "$work/headless.cfg" <<CFG
video_fullscreen = "false"
video_driver = "gl"
audio_driver = "null"
input_driver = "x"
input_joypad_driver = "null"
menu_driver = "rgui"
screenshot_directory = "$work/screenshots"
video_window_save_positions = "false"
CFG

export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe
unset WAYLAND_DISPLAY  # the check must run on the Xvfb display, like the sandbox does
"$RETROARCH" -v --config "$config" --appendconfig "$work/headless.cfg" -L "$CORE" "$work/roms/gb/pattern.semu" > "$work/retroarch.log" 2>&1 &
pid=$!

# A command that lands during startup segfaults RetroArch 1.22, so wait for its
# command interface, then give content loading a grace period.
for _ in $(seq 1 60); do
  grep -q 'bringing_up_command_interface' "$work/retroarch.log" 2>/dev/null && break
  kill -0 "$pid" 2>/dev/null || break
  sleep 0.5
done
sleep 3

# GET_STATUS segfaults RetroArch 1.22 for a core without a core-info entry, so readiness is VERSION.
version=""
for _ in $(seq 1 60); do
  version="$(command_port VERSION || true)"
  [ -n "$version" ] && break
  kill -0 "$pid" 2>/dev/null || break
  sleep 0.5
done
if [ -z "$version" ]; then
  echo "retroarch never answered VERSION" >&2
  grep -i -E 'video|\[gl\]|egl|glx|error|driver|netcmd|segmentation' "$work/retroarch.log" | grep -v GameMode | tail -30 >&2
  kill "$pid" 2>/dev/null || true
  exit 1
fi
echo "retroarch version: $version"

command_port SCREENSHOT >/dev/null || true
shot=""
for _ in $(seq 1 20); do
  shot="$(find "$work/screenshots" -name '*.png' | head -1)"
  [ -n "$shot" ] && [ -s "$shot" ] && break
  sleep 0.5
done
[ -n "$shot" ] || { echo "no screenshot written" >&2; kill "$pid" 2>/dev/null || true; exit 1; }
mean="$(magick identify -format '%[fx:mean]' "$shot")"
echo "screenshot: $shot mean=$mean"
awk "BEGIN { exit !($mean > 0.05) }" || { echo "screenshot is blank" >&2; kill "$pid" 2>/dev/null || true; exit 1; }

for attempt in 1 2 3; do  # RetroArch occasionally drops the first QUIT
  command_port QUIT >/dev/null || true
  for _ in $(seq 1 10); do
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.5
  done
  kill -0 "$pid" 2>/dev/null || break
done
if kill -0 "$pid" 2>/dev/null; then
  echo "retroarch ignored QUIT" >&2
  kill "$pid" 2>/dev/null || true
  exit 1
fi
status=0
wait "$pid" || status=$?
[ "$status" -eq 0 ] || { echo "retroarch exited $status after QUIT" >&2; exit 1; }
echo "retroarch-headless: pass"
