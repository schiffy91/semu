#!/bin/bash
# Controller input on the Steam Deck, off-screen: each case launches a game the way ES-DE does,
# inside a private headless gamescope at 1280x800 with the sound cut (as system-matrix.sh), while a
# virtual gamepad on /dev/uinput (tests/visual/virtual_pad.btrc, built for x86_64-linux) presses
# the case's buttons. The pad is a replica of Steam's virtual pad (--steam-virtual-pad), and Steam's
# own pad is hidden from the game (its device nodes covered by /dev/null in a bubblewrap around
# gamescope, which also gets its own /tmp/.X11-unix: in that user namespace the host's is owned
# by nobody, and Xwayland refuses it), so the replica is the first and only pad, with the GUID and name every emulator's
# compiled device identity names, as in Game Mode. SDL ignores a Steam virtual pad unless
# SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD=1, which Steam sets for every game it launches, so
# the case sets it too, with the SteamVirtualGamepadInfo slot file through which SDL names pad 0
# "Steam Deck Controller" (the name ES-DE logs in Game Mode, and Dolphin's compiled device). The
# Deck's own controls (28de:1205, hidraw) are hidden as Steam hides them from games, or SDL's HIDAPI
# driver lists them first under the same name. Dolphin's Wii remotes read those controls directly
# (its SteamDeck backend), so Wii input cannot be checked this way. Steam ignores that replica (its own virtual
# pad's ids); a pad Steam adopts would drive Steam's UI instead (a plain test pad once pressed A
# on Semu in the library), so the case stops the pad before its first press if Steam's controller
# log shows it found a new device, and the run ends. The game is captured just before the first press and a few seconds after
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
# seconds (default 6, SEMU_INPUT_GAP); a second case of one SYSTEM-EMULATOR[-CORE] gets a -2 suffix.
# OUT/<case>/ gets before.png, after-<n>-<button>.png, cmdline (the emulator argv), run.log
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
printf "[slot 0]\nVID=0x28de\nPID=0x1205\ntype=steamdeck\nname=Steam Deck Controller\nhandle=1\n" > "$out/steam-virtual-gamepad-info"  # as Steam writes it: SDL names "pad 0" after slot 0

descendants() { local child; for child in $(pgrep -P "$1"); do echo "$child"; descendants "$child"; done; }
battery() { cat /sys/class/power_supply/BAT1/capacity 2>/dev/null || echo 100; }
charging() { grep -q -E 'Charging|Full' /sys/class/power_supply/BAT1/status 2>/dev/null; }

while IFS= read -r line; do
  case "$line" in ''|'#'*) continue ;; esac
  head="${line%% -- *}"; pattern="${line#* -- }"
  read -r system emulator core first buttons <<< "$head"
  name="$system-$emulator"; [ "$core" = - ] || name="$name-$core"
  base="$name"; suffix=2; while [ -e "$out/$name" ]; do name="$base-$suffix"; suffix=$((suffix + 1)); done  # a second case of one emulator keeps its own results
  dir="$out/$name"; mkdir -p "$dir"; : > "$dir/result"
  note() { echo "$*" >> "$dir/result"; }
  if [ "$(battery)" -lt 20 ] && ! charging; then note "skipped: battery $(battery)% and not charging"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; continue; fi
  for pid in $(pgrep -f "/(es-de|$emulators)([^/]*)( |$)" 2>/dev/null); do  # the owner is playing: leave the Deck to them
    tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null | grep -q '^SEMU_MATRIX_ROM=' && continue
    note "stopped: the owner's $(cat "/proc/$pid/comm" 2>/dev/null) (pid $pid) is running"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; date > "$out/done"; exit 0
  done
  rom="$(compgen -G "$roms/$system/$pattern" | head -1)"
  if [ -z "$rom" ]; then note "skipped: no ROM matches $system/$pattern"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; continue; fi
  note "rom: ${rom#"$roms/"} (battery $(battery)% $(cat /sys/class/power_supply/BAT1/status 2>/dev/null))"
  core_argument=""; [ "$core" = - ] || core_argument="--core $core"
  printf '#!/bin/sh\nexec "%s" launch %s --system %s %s %s --rom "$SEMU_MATRIX_ROM"\n' "$cli" "$emulator" "$system" "$core_argument" "${SEMU_INPUT_LAUNCH_ARGS:-}" > "$dir/inner.sh"  # SEMU_INPUT_LAUNCH_ARGS: e.g. --project DIR for a trial config
  chmod +x "$dir/inner.sh"

  tokens=""; for button in $buttons; do tokens="$tokens press:$button sleep:$gap"; done
  replica="--steam-virtual-pad"; cover=""
  for node in /sys/class/input/event* /sys/class/input/js*; do  # Steam's own pad, found before the replica exists
    [ "$(cat "$node/device/id/vendor" 2>/dev/null)" = 28de ] && [ "$(cat "$node/device/id/product" 2>/dev/null)" = 11ff ] && cover="$cover --ro-bind /dev/null /dev/input/${node##*/}"
  done
  for node in /sys/class/hidraw/hidraw*; do  # the Deck's own controls, which Steam hides from games (SDL_GAMECONTROLLER_IGNORE_DEVICES)
    grep -q -i "HID_ID=.*000028DE:00001205" "$node/device/uevent" 2>/dev/null && cover="$cover --ro-bind /dev/null /dev/${node##*/}"
  done
  note "pad: replica of Steam's virtual pad; hidden:${cover:- nothing}"
  steamlog="$HOME/.local/share/Steam/logs/controller.txt"
  logsize=$(stat -c %s "$steamlog" 2>/dev/null || echo 0)
  "$pad" $replica "$first" $tokens > "$dir/pad.log" 2>&1 &  # present before the emulator starts, so no hotplug is needed
  padpid=$!
  start=$(date +%s)
  SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD=1 SDL_GAMECONTROLLER_IGNORE_DEVICES=0x28de/0x1205 SteamVirtualGamepadInfo="$out/steam-virtual-gamepad-info" PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy ALSA_CONFIG_PATH="$out/alsa-null.conf" SEMU_MATRIX_ROM="$rom" \
    bwrap --dev-bind / / --tmpfs /tmp/.X11-unix $cover -- gamescope --backend headless -W 1280 -H 800 -w 1280 -h 800 -- "$dir/inner.sh" > "$dir/run.log" 2>&1 &
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
  while [ $(( $(date +%s) - start )) -lt $(( first - 4 )) ]; do sleep 1; done
  if tail -c +$(( logsize + 1 )) "$steamlog" 2>/dev/null | grep -a -q -E "Local Device Found|Created virtual controller"; then  # Steam took the pad: no press may reach its UI
    kill -TERM "$padpid" 2>/dev/null
    note "aborted: Steam adopted the test pad before its first press"
    [ -n "$game" ] && kill -TERM "$game" 2>/dev/null
    { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; date > "$out/done"; exit 1
  fi
  leaf="$game"; while [ -n "$leaf" ] && child=$(pgrep -P "$leaf" | tail -1) && [ -n "$child" ]; do leaf=$child; done
  [ -n "$leaf" ] && tr '\0' '\n' < "/proc/$leaf/cmdline" > "$dir/cmdline" 2>/dev/null && note "argv-last: $(tail -1 "$dir/cmdline")"  # the file the emulator was told to open
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
