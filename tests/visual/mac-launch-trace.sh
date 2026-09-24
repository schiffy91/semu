#!/usr/bin/env bash
# How each macOS system's emulator window appears at launch, recorded while the screen is locked so
# nothing reaches the owner's display. Each launch is semu's own plan (`--print-plan`); window-trace
# logs the emulator's windows (bounds, alpha, layer) every 15 ms and captures the last one alone. A
# smooth launch shows one window at the full screen size, transparent until it is fullscreen, never
# a small window or a titled one.
# The library is read-only; everything written goes to OUT_DIR.
# usage: mac-launch-trace.sh BUNDLE OUT_DIR SECONDS system...   (refuses to run while unlocked)
set -uo pipefail
bundle="$(cd "$1" && pwd -P)"; out="$2"; seconds="$3"; shift 3
locked() { ioreg -n Root -d1 -a | grep -A1 CGSSessionScreenIsLocked | tail -1 | grep -q '<true/>'; }
locked || { echo "mac-launch-trace: the screen is unlocked; lock it first, this never draws on the owner's display" >&2; exit 2; }
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$out"
cc -O2 -framework CoreGraphics -framework CoreFoundation "$here/window-trace.c" -o "$out/window-trace" || exit 1
config="$bundle/share/semu/config"
roms="$("$bundle/bin/semu" path roms --target macos)"
for system in "$@"; do
  emulator="$(jq -r '[.emulators[]? | select((.platforms // ["macos"]) | index("macos")) | .emulator][0] // empty' "$config/systems/$system/system.json")"
  [ -n "$emulator" ] || { echo "$system: no macOS emulator"; continue; }
  directory="$roms/$(jq -r '.rom.dir' "$config/systems/$system/system.json")"
  rom="$(ls "$directory" 2>/dev/null | grep -viE '\.(txt|xml|json|png|jpg|pak|srm|sav|eep|mpk|ram)$' | head -1)"
  [ -n "$rom" ] || { echo "$system: no ROM in $directory"; continue; }
  work="$out/$system"; rm -rf "$work"; mkdir -p "$work/content"
  "$bundle/bin/semu" launch "$emulator" --system "$system" --rom "$rom" --target macos --asset-root "$bundle" --semu-home "$work/home" \
    --settings-json "{\"paths\":{\"state_root\":\"$work/state\",\"content_root\":\"$work/content\"}}" --print-plan > "$work/plan.json" \
    || { echo "$system: no launch plan"; continue; }
  mapfile -t command < <(jq -r '.argv[]' "$work/plan.json")
  mapfile -t variables < <(jq -r '.environment[]' "$work/plan.json")
  env "${variables[@]}" "${command[@]}" > "$work/emulator.log" 2>&1 &
  pid=$!
  "$out/window-trace" "$pid" "$seconds" > "$work/windows.txt"
  window="$(tail -1 "$work/windows.txt" | sed -nE "s/.* id ([0-9]+)\]$/\1/p")"
  [ -n "$window" ] && screencapture -x -o -l "$window" "$work/window.png" 2>/dev/null  # the window alone, never the display
  locked || { kill -KILL "$pid" 2>/dev/null; echo "mac-launch-trace: the screen was unlocked mid-run; stopped" >&2; exit 2; }
  kill -TERM "$pid" 2>/dev/null
  for _ in $(seq 1 40); do kill -0 "$pid" 2>/dev/null || break; sleep 0.1; done
  kill -KILL "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
  echo "== $system ($emulator, $rom)"; cat "$work/windows.txt"
done
