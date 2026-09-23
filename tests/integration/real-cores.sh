#!/bin/sh
# Real cores with real (freely licensed) test programs through `semu launch`: each boots, draws a
# frame that is not blank, saves a state file, loads it back, and quits through RetroArch's QUIT.
# Inputs: SEMU_CLI, RETROARCH, RENDERER (renderer lib dir), CASES (lines: system core rom-path core-path).
set -eu
work="${TMPDIR:-/tmp}/semu-real-cores"
command_port() { printf '%s' "$1" | socat -t 1 - UDP:127.0.0.1:55355 2>/dev/null | tr -d '\n'; }
config="$(dirname "$(readlink -f "$SEMU_CLI")")/../share/semu/config"
rm -rf "$work"
mkdir -p "$work/home" "$work/assets/bin" "$work/assets/lib/retroarch/cores"
export HOME="$work/home" LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
unset WAYLAND_DISPLAY
ln -s "$RETROARCH" "$work/assets/bin/retroarch"
ln -s "$RENDERER/libsemurenderer.so" "$work/assets/lib/libsemurenderer.so"
settings="{\"paths\":{\"roms\":\"$work/roms\",\"state_root\":\"$work/state\",\"content_root\":\"$work/content\"}}"
failures=0
echo "$CASES" | while read -r system core rom corefile; do
  [ -n "$system" ] || continue
  ln -sf "$corefile" "$work/assets/lib/retroarch/cores/${core}_libretro.so"
  romDirectory="$work/roms/$(jq -r '.rom.dir' "$config/systems/$system/system.json")"
  mkdir -p "$romDirectory"
  cp "$rom" "$romDirectory/"
  name="$(basename "$rom")"
  "$SEMU_CLI" launch retroarch --system "$system" --rom "$name" --asset-root "$work/assets" --settings-json "$settings" > "$work/$system.log" 2>&1 &
  launcher=$!
  version=""
  for _ in $(seq 1 80); do version="$(command_port VERSION || true)"; [ -n "$version" ] && break; sleep 0.5; done
  sleep 4  # let the program reach its first screen
  command_port SCREENSHOT >/dev/null || true
  command_port SAVE_STATE >/dev/null || true
  sleep 2
  command_port LOAD_STATE >/dev/null || true
  sleep 1
  shot="$(find "$work/content/screenshots" -name '*.png' -newer "$work/$system.log" 2>/dev/null | head -1)"
  state="$(find "$work/content/states" -name '*.state*' -newer "$work/$system.log" 2>/dev/null | head -1)"
  kill -TERM "$launcher" 2>/dev/null || true
  wait "$launcher" || true
  spread="$([ -n "$shot" ] && magick identify -format '%[fx:standard_deviation]' "$shot" || echo 0)"
  if [ -z "$version" ]; then echo "$system ($core): RetroArch never answered"; tail -5 "$work/$system.log"; echo x >> "$work/failed"
  elif ! awk "BEGIN { exit !($spread > 0.02) }"; then echo "$system ($core): the frame is blank (spread $spread, ${shot:-no screenshot})"; echo x >> "$work/failed"
  elif [ -z "$state" ]; then echo "$system ($core): SAVE_STATE wrote no state file"; echo x >> "$work/failed"
  elif ! grep -q 'honoured QUIT' "$work/$system.log"; then echo "$system ($core): did not honour QUIT"; echo x >> "$work/failed"
  else echo "$system ($core): booted $name, frame spread $spread, state $(basename "$state"), quit cleanly"; fi
  rm -f "$work/content/screenshots/"*.png
done
[ ! -f "$work/failed" ] && echo "real-cores: pass" || { echo "real-cores: $(wc -l < "$work/failed") failed"; exit 1; }
