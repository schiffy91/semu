#!/bin/bash
# Controller input on the Steam Deck, off-screen: each case launches a game the way ES-DE does,
# inside a private headless gamescope at 1280x800 with the sound cut (as system-matrix.sh), while a
# virtual gamepad on /dev/uinput (tests/visual/virtual_pad.btrc, built for x86_64-linux) presses
# the case's buttons. The game is captured just before the first press and a few seconds after
# each press, so the pictures show whether the emulator acted on the pad. Quits the way Semu quits
# (SIGTERM to semu-btrc). Everything is tracked by PID; the script writes only below OUT and
# removes nothing.
#
#   input-check.sh PAD CASES OUT
#
# PAD is the virtual_pad binary. CASES has one case per line:
#   SYSTEM EMULATOR CORE|- FIRST BUTTON... -- ROM-GLOB
# FIRST is the second after launch of the first press; buttons are virtual_pad names (south, east,
# north, west, tl, tr, select, start, dpad_up, dpad_down, dpad_left, dpad_right), one every GAP
# seconds (default 6, SEMU_INPUT_GAP). OUT/<case>/ gets before.png, after-<n>-<button>.png, run.log
# and result; OUT/summary collects every result; OUT/done marks the end.
set -u
pad="$1"; cases="$2"; out="$3"
gap="${SEMU_INPUT_GAP:-6}"
roms=/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
cli="$HOME/Applications/Semu/bin/semu-deck-cli"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
emulators='retroarch|azahar|dolphin|pcsx2|ppsspp|melonds|flycast|cemu|ryujinx|semu-btrc'
mkdir -p "$out"
printf "pcm.!default {\n  type null\n}\nctl.!default {\n  type hw\n  card 0\n}\n" > "$out/alsa-null.conf"

descendants() { local child; for child in $(pgrep -P "$1"); do echo "$child"; descendants "$child"; done; }
battery() { cat /sys/class/power_supply/BAT1/capacity 2>/dev/null || echo 100; }
charging() { grep -q -E 'Charging|Full' /sys/class/power_supply/BAT1/status 2>/dev/null; }

while IFS= read -r line; do
  case "$line" in ''|'#'*) continue ;; esac
  head="${line%% -- *}"; pattern="${line#* -- }"
  read -r system emulator core first buttons <<< "$head"
  name="$system-$emulator"; [ "$core" = - ] || name="$name-$core"
  dir="$out/$name"; mkdir -p "$dir"; : > "$dir/result"
  note() { echo "$*" >> "$dir/result"; }
  if [ "$(battery)" -lt 15 ] && ! charging; then note "skipped: battery $(battery)% and not charging"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; continue; fi
  for pid in $(pgrep -f "/(es-de|$emulators)([^/]*)( |$)" 2>/dev/null); do  # the owner is playing: leave the Deck to them
    tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null | grep -q '^SEMU_MATRIX_ROM=' && continue
    note "stopped: the owner's $(cat "/proc/$pid/comm" 2>/dev/null) (pid $pid) is running"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; date > "$out/done"; exit 0
  done
  rom="$(compgen -G "$roms/$system/$pattern" | head -1)"
  if [ -z "$rom" ]; then note "skipped: no ROM matches $system/$pattern"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; continue; fi
  note "rom: ${rom#"$roms/"}"
  core_argument=""; [ "$core" = - ] || core_argument="--core $core"
  printf '#!/bin/sh\nexec "%s" launch %s --system %s %s --rom "$SEMU_MATRIX_ROM"\n' "$cli" "$emulator" "$system" "$core_argument" > "$dir/inner.sh"
  chmod +x "$dir/inner.sh"

  tokens=""; for button in $buttons; do tokens="$tokens press:$button sleep:$gap"; done
  "$pad" "$first" $tokens > "$dir/pad.log" 2>&1 &  # present before the emulator starts, so no hotplug is needed
  padpid=$!
  start=$(date +%s)
  PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy ALSA_CONFIG_PATH="$out/alsa-null.conf" SEMU_MATRIX_ROM="$rom" \
    gamescope --backend headless -W 1280 -H 800 -w 1280 -h 800 -- "$dir/inner.sh" > "$dir/run.log" 2>&1 &
  headless=$!
  game=""; display=""
  for _ in $(seq 1 60); do
    for pid in $(descendants "$headless"); do
      [ -z "$display" ] && display="$(tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null | sed -n 's/^GAMESCOPE_WAYLAND_DISPLAY=//p')"
      [ "$(cat "/proc/$pid/comm" 2>/dev/null)" = semu-btrc ] && game=$pid
    done
    [ -n "$game" ] && [ -n "$display" ] && break
    kill -0 "$headless" 2>/dev/null || break
    sleep 1
  done
  [ -n "$game" ] && note "semu-btrc: pid $game after $(( $(date +%s) - start )) s" || note "semu-btrc: never started"

  capture() {  # capture NAME AT: waits until AT seconds after launch, then screenshots
    while [ $(( $(date +%s) - start )) -lt "$2" ]; do sleep 1; done
    local shot="$dir/$1.png"
    [ -n "$display" ] && GAMESCOPE_WAYLAND_DISPLAY="$display" timeout 20 gamescopectl screenshot "$shot" > /dev/null 2>&1
    for _ in $(seq 1 10); do [ -s "$shot" ] && break; sleep 1; done
    local running=no; [ -n "$game" ] && kill -0 "$game" 2>/dev/null && running=yes
    note "$1 at t=$(( $(date +%s) - start )) running=$running shot=$([ -s "$shot" ] && echo yes || echo no)"
  }
  capture before $(( first - 2 ))
  index=1; at=$(( first + gap - 1 ))
  for button in $buttons; do capture "after-$index-$button" "$at"; index=$(( index + 1 )); at=$(( at + gap )); done

  if [ -n "$game" ] && kill -0 "$game" 2>/dev/null; then
    kill -TERM "$game"
    for _ in $(seq 1 20); do kill -0 "$game" 2>/dev/null || break; sleep 1; done
    kill -0 "$game" 2>/dev/null && note "quit: semu-btrc still running 20 s after SIGTERM" || note "quit: clean"
  fi
  for _ in $(seq 1 15); do kill -0 "$headless" 2>/dev/null || break; sleep 1; done
  kill -0 "$headless" 2>/dev/null && { kill -TERM "$headless"; note "headless gamescope needed a SIGTERM"; }
  kill -0 "$padpid" 2>/dev/null && kill -TERM "$padpid"
  wait "$padpid" 2>/dev/null
  { echo "== $name"; cat "$dir/result"; } >> "$out/summary"
done < "$cases"
date > "$out/done"
