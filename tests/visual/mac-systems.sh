#!/usr/bin/env bash
# Every RetroArch system on this Mac through `semu launch --target macos`, one real ROM each from the
# library (read only; saves, states and screenshots go to OUT_DIR): the command port answers, the
# screen is captured with the Semu renderer's composition, a state saves and loads, and stopping the
# launcher ends RetroArch through its own QUIT. usage: mac-systems.sh ASSET_ROOT OUT_DIR [system...]
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
assets="$1"; out="$2"; shift 2
socat="$(nix build --no-warn-dirty --no-link --print-out-paths --inputs-from "$root" nixpkgs#socat)/bin/socat"
port() { printf '%s' "$1" | "$socat" -t 1 - UDP:127.0.0.1:55355 2>/dev/null | tr -d '\n'; }
roms="$("$root/build/semu" path roms --target macos --project "$root/config")"
systems=("$@")
[ ${#systems[@]} -gt 0 ] || systems=(gb gbc gba nes snes genesis n64 psx nds psp n3ds)
mkdir -p "$out"
failures=0
for system in "${systems[@]}"; do
  directory="$roms/$(jq -r '.rom.dir' "$root/config/systems/$system/system.json")"
  rom="$(ls "$directory" 2>/dev/null | grep -viE '\.(txt|xml|json|png|jpg)$' | head -1)"
  [ -n "$rom" ] || { echo "$system: no ROM in $directory"; failures=$((failures + 1)); continue; }
  work="$out/$system"; rm -rf "$work"; mkdir -p "$work"
  "$root/build/semu" launch retroarch --system "$system" --rom "$rom" --target macos --project "$root/config" --asset-root "$assets" \
    --settings-json "{\"paths\":{\"state_root\":\"$work/state\",\"content_root\":\"$work/content\"}}" > "$work/launch.log" 2>&1 &
  launcher=$!
  version=""
  for _ in $(seq 1 80); do version="$(port VERSION)"; [ -n "$version" ] && break; kill -0 "$launcher" 2>/dev/null || break; sleep 0.5; done
  sleep "${SEMU_BOOT_WAIT:-10}"
  screencapture -x "$work/screen.png"
  port SCREENSHOT >/dev/null; port SAVE_STATE >/dev/null; sleep 2; port LOAD_STATE >/dev/null; sleep 1
  kill -TERM "$launcher" 2>/dev/null; wait "$launcher" 2>/dev/null
  state="$(find "$work/content/states" -type f 2>/dev/null | head -1)"
  shot="$(find "$work/content/screenshots" -name '*.png' 2>/dev/null | head -1)"
  if [ -z "$version" ]; then echo "$system: RetroArch never answered ($rom)"; tail -3 "$work/launch.log"; failures=$((failures + 1))
  elif ! grep -q 'honoured QUIT' "$work/launch.log"; then echo "$system: did not honour QUIT ($rom)"; failures=$((failures + 1))
  else echo "$system: $rom, screenshot ${shot:+yes}${shot:-no}, state ${state:+yes}${state:-no}, quit cleanly"; fi
done
exit "$failures"
