#!/bin/bash
# Controller, radial and trackpad input on the Steam Deck, off-screen: each case launches a game the
# way ES-DE does, inside a private headless gamescope at 1280x800 with the sound cut (as
# system-matrix.sh), while a virtual gamepad on /dev/uinput (tests/visual/virtual_pad.btrc, built
# for x86_64-linux) presses the case's buttons and inject.sh types its radial chords and moves and
# clicks its pointer. The pad is a replica of Steam's virtual pad (--steam-virtual-pad), and Steam's
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
# each token, so the pictures show whether the emulator acted. Quits the way Semu quits
# (SIGTERM to semu-btrc). Everything is tracked by PID; the script itself writes only below OUT
# (what Semu and the emulators write is under "Isolation") and removes nothing.
#
#   input-check.sh PAD CASES OUT
#   input-check.sh --plan CASES      # each case's pad and injector timelines and captures; launches nothing (runs on a Mac)
#
# PAD is the virtual_pad binary. CASES has one case per line:
#   SYSTEM EMULATOR CORE|- FIRST TOKEN... -- ROM-GLOB
# Token I (from 0) fires FIRST + I*GAP seconds after launch on one clock that the pad and inject.sh
# share (GAP default 6, SEMU_INPUT_GAP):
#   NAME        the pad presses a button: south, east, north, west, tl, tr, tl2, tr2, select, start,
#               mode, thumbl, thumbr, dpad_up, dpad_down, dpad_left, dpad_right (positions: north is
#               the top face button, which the replica sends as BTN_WEST, as Steam's pad does)
#   A+B         a pad chord: hold A, press B, release A (A+B+C holds A and B)
#   key:CHORD   inject.sh types CHORD as XTest from the second Xwayland, the way Steam's radial sends a slot
#   move:FX,FY  inject.sh moves the pointer to FX,FY of the touch screen as drawn, by relative motion as
#               Steam's trackpad mouse does (see inject.sh)
#   tap:FX,FY   the same, then a 0.15 s click
# The pad sleeps through X tokens and inject.sh through pad tokens; a press's own time comes off
# its pause, so neither drifts. A second case of one SYSTEM-EMULATOR[-CORE] gets a -2 suffix.
#
# gamescope runs with --xwayland-count 2, the game on the first display and inject.sh typing from
# the second, so its keys and pointer reach the game only through gamescope, as Steam's XTest does,
# and with Game Mode's --hide-cursor-delay 3000. inject.sh types only into displays it proves are
# this gamescope's and numbered 2 or higher, and refuses otherwise (see inject.sh). The launch gets
# SEMU_RENDER_DEBUG=1 (Semu's and the renderer's input lines) and SEMU_RENDER_CAPTURE_FRAME=60 (a
# receipt of the drawn screens before the first token, which move and tap read).
#
# Isolation, exactly: every case runs with --semu-home OUT/home, whose semu.json (made once per OUT)
# moves only paths.content_root, to OUT/content. The owner's semu.json, RetroArch saves, states and
# screenshots are untouched, and the bezel and shader choices a case makes land in OUT/home/semu.json,
# where the next cases of the run start from them. The state root stays the owner's
# (~/.local/share/semu/<emulator>): the session's action journal, the variants file, the render
# receipts and the frame-60 capture go there, every launch rewrites the compiled emulator configs
# there (the owner's next launch writes them back), and a standalone emulator keeps its own saves,
# states and settings there. So no case saves or loads a state on a standalone emulator.
#
# OUT/<case>/ gets before.png, after-<n>-<token>.png (':' written '-', ',' written '_'), and for a
# move or tap cursor-<n>.png 0.5 s after it and idle-<n>.png 4 s after it, both gamescope screenshot
# type 3 (every layer, gamescope's cursor plane included; gamescope 3.16.30 takes the type as the
# screenshot command's third argument, the other shots keep the default base plane); schedule;
# cmdline (the emulator argv, read the moment it is first seen); run.log; inject.log; touch.log
# (Semu's touch lines); journal.od (the action journal, od -A d -t d4, one 56-byte record a line);
# evidence.log (this launch's receipts); and result. OUT/summary collects every result; OUT/done
# marks the end.
set -u
here="$(cd "$(dirname "$0")" && pwd -P)"
inject="$here/inject.sh"
gap="${SEMU_INPUT_GAP:-6}"
buttons="south east north west tl tr tl2 tr2 select start mode thumbl thumbr dpad_up dpad_down dpad_left dpad_right"

known() { case " $buttons " in *" $1 "*) return 0 ;; esac; return 1; }
at_ms() {  # FIRST STEP OFFSET: milliseconds after launch of FIRST + STEP*GAP + OFFSET seconds
  awk -v first="$1" -v step="$2" -v offset="$3" -v gap="$gap" 'BEGIN { printf "%d", (first + step * gap + offset) * 1000 + 0.5 }'
}
label() { printf '%s' "$1" | tr ':,' '-_'; }
pad_steps() {  # TOKEN: "SECONDS STEP..." (virtual_pad's tokens for it and the time they take), or fails on a name the pad lacks
  local rest="$1" name held="" released="" count=1
  while :; do
    name="${rest%%+*}"
    known "$name" || return 1
    [ "$name" = "$rest" ] && break
    held="${held}hold:$name "; released=" release:$name$released"; count=$((count + 1)); rest="${rest#*+}"
  done
  echo "$(awk -v count="$count" 'BEGIN { printf "%g", count * 0.2 }') ${held}press:$name$released"  # hold 0.08 s, press 0.2 s, release 0.12 s
}
pad_arguments() {  # FIRST TOKEN...: virtual_pad's arguments after --steam-virtual-pad
  local arguments="$1" token steps pause
  shift
  for token in "$@"; do
    case "$token" in
      key:*|move:*|tap:*) arguments="$arguments sleep:$(awk -v gap="$gap" 'BEGIN { printf "%g", gap }')" ;;
      *:*) echo "input-check: '$token' is not a pad button or key:, move:, tap:" >&2; return 2 ;;
      *) steps="$(pad_steps "$token")" || { echo "input-check: '$token' names a button the pad lacks ($buttons)" >&2; return 2; }
         pause="$(awk -v gap="$gap" -v used="${steps%% *}" 'BEGIN { if (gap < used) exit 1; printf "%g", gap - used }')" || { echo "input-check: GAP $gap is shorter than '$token'" >&2; return 2; }
         arguments="$arguments ${steps#* } sleep:$pause" ;;
    esac
  done
  echo "$arguments"
}
schedule() {  # FIRST TOKEN...: "MS pad TOKEN: STEPS" and "MS shot NAME [TYPE]" lines in time order (inject.sh --plan adds its own)
  local first="$1" step=0 token at
  shift
  echo "$(at_ms "$first" 0 -2) shot before"
  for token in "$@"; do
    at="$(at_ms "$first" "$step" 0)"
    case "$token" in
      key:*) ;;
      move:*|tap:*)
        echo "$((at + 500)) shot cursor-$((step + 1)) 3"
        awk -v gap="$gap" 'BEGIN { exit !(gap >= 6) }' && echo "$((at + 4000)) shot idle-$((step + 1)) 3" ;;
      *) echo "$at pad $token: $(pad_steps "$token" | cut -d' ' -f2-)" ;;
    esac
    echo "$(at_ms "$first" $((step + 1)) -1) shot after-$((step + 1))-$(label "$token")"
    step=$((step + 1))
  done
}
x_tokens() { local token; for token in "$@"; do case "$token" in key:*|move:*|tap:*) return 0 ;; esac; done; return 1; }
plain() { case "$1" in *[!A-Za-z0-9_+:,.\ ]*) echo "input-check: a token holds a character outside A-Z a-z 0-9 _ + : , ." >&2; return 2 ;; esac; }  # before any unquoted use: no glob, no shell syntax in inner.sh

if [ "${1:-}" = --plan ]; then  # the timelines a run would follow, from the same functions and inject.sh's own parser
  cases="${2:?usage: input-check.sh --plan CASES}"; status=0; seen=" "
  while IFS= read -r line; do
    case "$line" in ''|'#'*) continue ;; esac
    head="${line%% -- *}"; pattern="${line#* -- }"
    read -r system emulator core first tokens <<< "$head"
    name="$system-$emulator"; [ "$core" = - ] || name="$name-$core"
    base="$name"; suffix=2; while case "$seen" in *" $name "*) true ;; *) false ;; esac; do name="$base-$suffix"; suffix=$((suffix + 1)); done
    seen="$seen$name "
    echo "== $name: $system/$pattern"
    if ! plain "$tokens" || ! arguments="$(pad_arguments "$first" $tokens)" || ! injected="$(bash "$inject" --plan "$first" "$gap" $tokens)"; then
      echo "   not run: a token above is malformed"; status=2; continue
    fi
    echo "pad    virtual_pad --steam-virtual-pad $arguments"
    if x_tokens $tokens; then echo "inject inject.sh $first $gap $tokens"; else echo "inject none (no key, move or tap token)"; fi
    { schedule "$first" $tokens; [ -z "$injected" ] || printf '%s\n' "$injected"; } | sort -n -s -k1,1 | awk '{
        text = ""; for (field = 3; field <= NF; field++) text = text (field > 3 ? " " : "") $field
        if ($2 == "shot" && NF == 4) text = $3 " (type " $4 ")"
        printf "%8.1f  %-6s %s\n", $1 / 1000, $2, text }'
  done < "$cases"
  exit "$status"
fi

pad="$1"; cases="$2"; out="$(mkdir -p "$3" && cd "$3" && pwd -P)"
roms=/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
cli="$HOME/Applications/Semu/bin/semu-deck-cli"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
emulators='retroarch|azahar|dolphin|pcsx2|ppsspp|melonds|flycast|cemu|ryujinx|semu-btrc'
printf "pcm.!default {\n  type null\n}\nctl.!default {\n  type hw\n  card 0\n}\n" > "$out/alsa-null.conf"
printf "[slot 0]\nVID=0x28de\nPID=0x1205\ntype=steamdeck\nname=Steam Deck Controller\nhandle=1\n" > "$out/steam-virtual-gamepad-info"  # as Steam writes it: SDL names "pad 0" after slot 0
home="$out/home"; content="$out/content"
mkdir -p "$home" "$content/saves" "$content/states" "$content/screenshots"
[ -s "$home/semu.json" ] || jq -n --arg content "$content" '{paths: {content_root: $content}}' > "$home/semu.json"

descendants() { local child; for child in $(pgrep -P "$1"); do echo "$child"; descendants "$child"; done; }
battery() { cat /sys/class/power_supply/BAT1/capacity 2>/dev/null || echo 100; }
charging() { grep -q -E 'Charging|Full' /sys/class/power_supply/BAT1/status 2>/dev/null; }
leaf_of() { local leaf="$1" child; while [ -n "$leaf" ] && child=$(pgrep -P "$leaf" | tail -1) && [ -n "$child" ]; do leaf=$child; done; echo "$leaf"; }
record_argv() {  # $1 the emulator: its argv once it has exec'd, silently nothing if it is already gone
  local leaf="$1"
  if [ -s "$dir/cmdline" ] || [ -z "$leaf" ] || [ "$leaf" = "$game" ] || [ "$(cat "/proc/$leaf/comm" 2>/dev/null)" = semu-btrc ]; then return 0; fi
  { tr '\0' '\n' < "/proc/$leaf/cmdline"; } > "$dir/cmdline" 2>/dev/null
  [ -s "$dir/cmdline" ] && note "argv-last: $(tail -1 "$dir/cmdline")"  # the file the emulator was told to open
  return 0
}
wait_until() {  # MS after launch on the shared clock
  local left=$(( start_ms + $1 - $(date +%s%3N) ))
  [ "$left" -gt 0 ] && sleep "$(awk -v left="$left" 'BEGIN { printf "%.3f", left / 1000 }')"
  return 0
}
evidence() {  # what the case left: the X key adapter, each action by source, the touches, the journal, the receipts, the saved choices
  note "x-listener: $(grep -o 'semu: listening for keys on X display [^ ]*' "$dir/run.log" | sed 's/.* //' | tr '\n' ' ')($(grep -c 'semu: listening for keys on X display' "$dir/run.log") lines)"
  grep -o 'semu: action [^ ]* ([a-z]*)' "$dir/run.log" | sort | uniq -c | while read -r count _ _ action source; do note "action: $action $source x$count"; done
  note "deduplicated: $(grep -c 'already ran from another input source' "$dir/run.log")"
  grep -E 'semu-retroarch: touch|semu-vulkan: touch' "$dir/run.log" > "$dir/touch.log"
  note "touch: $(grep -c 'semu-retroarch: touch' "$dir/touch.log") retroarch, $(grep -c 'semu-vulkan: touch' "$dir/touch.log") vulkan"
  head -12 "$dir/touch.log" | sed 's/^/touch-line: /' >> "$dir/result"
  if [ -n "$game" ] && [ -f "$state/semu-render-actions.bin" ]; then  # emptied when this session started
    od -A d -t d4 -w56 -v "$state/semu-render-actions.bin" > "$dir/journal.od"
    note "journal (action/slot; 1 menu, 2 up, 3 down, 4 confirm, 5 back, 6 save, 7 load, 9 screenshot, 77 next slot, 78 previous slot, 79 bezel, 80 shader): $(awk 'NF >= 10 { printf "%s%s/%s", separator, $8, $10; separator = " " }' "$dir/journal.od")"
  else
    note "journal: none (no session, or none at $state)"
  fi
  { tail -c +"$from" "$evidence"; } > "$dir/evidence.log" 2>/dev/null
  awk '{ delete value; for (field = 1; field <= NF; field++) { split($field, pair, "="); value[pair[1]] = pair[2] }
         art = value["bezel_art"]; sub(/.*\//, "", art); preset = value["shader_preset"]; sub(/.*\//, "", preset)
         printf "receipt: phase=%s bezel_art=%s shader_preset=%s layout=%s", value["phase"], art, preset, value["layout"]
         if (value["phase"] == "switch") printf " bezel_index=%s shader_index=%s reload_ms=%s", value["bezel_index"], value["shader_index"], value["reload_ms"]
         printf " surface1_content=%s\n", value["surface1_content"] }' "$dir/evidence.log" >> "$dir/result"
  note "choices: $(jq -c '.visual.systems // {}' "$home/semu.json" 2>/dev/null)"
  grep '^inject:' "$dir/inject.log" 2>/dev/null >> "$dir/result"
}

while IFS= read -r line; do
  case "$line" in ''|'#'*) continue ;; esac
  head="${line%% -- *}"; pattern="${line#* -- }"
  read -r system emulator core first tokens <<< "$head"
  name="$system-$emulator"; [ "$core" = - ] || name="$name-$core"
  base="$name"; suffix=2; while [ -e "$out/$name" ]; do name="$base-$suffix"; suffix=$((suffix + 1)); done  # a second case of one emulator keeps its own results
  dir="$out/$name"; mkdir -p "$dir"; : > "$dir/result"
  note() { echo "$*" >> "$dir/result"; }
  if ! arguments="$(plain "$tokens" 2>&1 && pad_arguments "$first" $tokens 2>&1)" || ! bash "$inject" --plan "$first" "$gap" $tokens > /dev/null 2> "$dir/plan-error"; then
    note "skipped: malformed case: $arguments $(cat "$dir/plan-error" 2>/dev/null)"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; continue
  fi
  schedule "$first" $tokens > "$dir/schedule"
  if [ "$(battery)" -lt 20 ] && ! charging; then note "skipped: battery $(battery)% and not charging"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; continue; fi
  for pid in $(pgrep -f "/(es-de|$emulators)([^/]*)( |$)" 2>/dev/null); do  # the owner is playing: leave the Deck to them
    { tr '\0' '\n' < "/proc/$pid/environ"; } 2>/dev/null | grep -q '^SEMU_MATRIX_ROM=' && continue
    note "stopped: the owner's $(cat "/proc/$pid/comm" 2>/dev/null) (pid $pid) is running"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; date > "$out/done"; exit 0
  done
  rom="$(compgen -G "$roms/$system/$pattern" | head -1)"
  if [ -z "$rom" ]; then note "skipped: no ROM matches $system/$pattern"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; continue; fi
  note "rom: ${rom#"$roms/"} (battery $(battery)% $(cat /sys/class/power_supply/BAT1/status 2>/dev/null))"
  state_root="$("$cli" path state_root --semu-home "$home" ${SEMU_INPUT_LAUNCH_ARGS:-} 2>/dev/null | tail -1)"
  state="$state_root/$emulator"; evidence="$state/semu-render-evidence.log"
  from=$(( $(stat -c %s "$evidence" 2>/dev/null || echo 0) + 1 ))  # this launch's receipts start here
  [ -n "$state_root" ] || note "state root: semu-deck-cli path state_root printed nothing"
  core_argument=""; [ "$core" = - ] || core_argument="--core $core"
  {
    echo '#!/bin/sh'
    x_tokens $tokens && printf '( bash "%s" %s %s %s > "%s/inject.log" 2>&1 & echo $! > "%s/inject.pid" )\n' "$inject" "$first" "$gap" "$tokens" "$dir" "$dir"  # a grandchild: never semu-btrc's child
    printf 'exec "%s" launch %s --system %s %s --semu-home "%s" %s --rom "$SEMU_MATRIX_ROM"\n' "$cli" "$emulator" "$system" "$core_argument" "$home" "${SEMU_INPUT_LAUNCH_ARGS:-}"  # SEMU_INPUT_LAUNCH_ARGS: e.g. --project DIR for a trial config
  } > "$dir/inner.sh"
  chmod +x "$dir/inner.sh"

  replica="--steam-virtual-pad"; cover=""
  for node in /sys/class/input/event* /sys/class/input/js*; do  # Steam's own pad, found before the replica exists
    [ "$(cat "$node/device/id/vendor" 2>/dev/null)" = 28de ] && [ "$(cat "$node/device/id/product" 2>/dev/null)" = 11ff ] && cover="$cover --ro-bind /dev/null /dev/input/${node##*/}"
  done
  for node in /sys/class/hidraw/hidraw*; do  # the Deck's own controls, which Steam hides from games (SDL_GAMECONTROLLER_IGNORE_DEVICES)
    grep -q -i "HID_ID=.*000028DE:00001205" "$node/device/uevent" 2>/dev/null && cover="$cover --ro-bind /dev/null /dev/${node##*/}"
  done
  note "pad: replica of Steam's virtual pad; hidden:${cover:- nothing}"
  note "gamescope: $(pacman -Q gamescope 2>/dev/null)"
  steamlog="$HOME/.local/share/Steam/logs/controller.txt"
  logsize=$(stat -c %s "$steamlog" 2>/dev/null || echo 0)
  start_ms=$(date +%s%3N); start=$(( start_ms / 1000 ))  # the shared clock's zero
  "$pad" $replica $arguments > "$dir/pad.log" 2>&1 &  # present before the emulator starts, so no hotplug is needed
  padpid=$!
  SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD=1 SDL_GAMECONTROLLER_IGNORE_DEVICES=0x28de/0x1205 SteamVirtualGamepadInfo="$out/steam-virtual-gamepad-info" PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy ALSA_CONFIG_PATH="$out/alsa-null.conf" SEMU_MATRIX_ROM="$rom" \
    SEMU_RENDER_DEBUG=1 SEMU_RENDER_CAPTURE_FRAME=60 SEMU_INJECT_START_MS="$start_ms" SEMU_INJECT_EVIDENCE="$evidence" SEMU_INJECT_EVIDENCE_FROM="$from" \
    bwrap --dev-bind / / --tmpfs /tmp/.X11-unix $cover -- gamescope --backend headless -W 1280 -H 800 -w 1280 -h 800 --xwayland-count 2 --hide-cursor-delay 3000 -- "$dir/inner.sh" > "$dir/run.log" 2>&1 &
  headless=$!
  game=""; display=""
  for _ in $(seq 1 300); do  # every 0.2 s for 60 s: an emulator that exits within a second is still seen
    for pid in $(descendants "$headless"); do
      [ -z "$display" ] && display="$({ tr '\0' '\n' < "/proc/$pid/environ"; } 2>/dev/null | sed -n 's/^GAMESCOPE_WAYLAND_DISPLAY=//p')"
      [ "$(cat "/proc/$pid/comm" 2>/dev/null)" = semu-btrc ] && game=$pid
    done
    record_argv "$(leaf_of "$game")"
    [ -n "$game" ] && [ -n "$display" ] && break
    kill -0 "$headless" 2>/dev/null || break
    sleep 0.2
  done
  [ -n "$game" ] && note "semu-btrc: pid $game after $(( $(date +%s) - start )) s" || note "semu-btrc: never started"
  for _ in $(seq 1 100); do  # the emulator's argv the moment it appears, before any press
    [ -n "$game" ] || break
    record_argv "$(leaf_of "$game")"
    [ -s "$dir/cmdline" ] && break
    kill -0 "$game" 2>/dev/null || break
    sleep 0.1
  done

  capture() {  # capture NAME AT_MS [TYPE]: waits until AT_MS after launch, then screenshots (TYPE 3: every layer, the cursor included)
    wait_until "$2"
    local shot="$dir/$1.png"
    if [ -n "$display" ]; then  # no display, no shot: never wait for one, or every later capture slips
      GAMESCOPE_WAYLAND_DISPLAY="$display" timeout 20 gamescopectl screenshot "$shot" ${3:+"$3"} > /dev/null 2>&1
      for _ in $(seq 1 10); do [ -s "$shot" ] && break; sleep 1; done
    fi
    local running=no; [ -n "$game" ] && kill -0 "$game" 2>/dev/null && running=yes
    note "$1 at t=$(awk -v now="$(date +%s%3N)" -v zero="$start_ms" 'BEGIN { printf "%.1f", (now - zero) / 1000 }') running=$running shot=$([ -s "$shot" ] && echo yes || echo no)${3:+ type=$3}"
  }
  while [ $(( $(date +%s) - start )) -lt $(( first - 4 )) ]; do sleep 1; done
  if tail -c +$(( logsize + 1 )) "$steamlog" 2>/dev/null | grep -a -q -E "Local Device Found|Created virtual controller"; then  # Steam took the pad: no press may reach its UI
    kill -TERM "$padpid" 2>/dev/null
    note "aborted: Steam adopted the test pad before its first press"
    [ -n "$game" ] && kill -TERM "$game" 2>/dev/null
    { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; date > "$out/done"; exit 1
  fi
  record_argv "$(leaf_of "$game")"
  [ -s "$dir/cmdline" ] || note "argv: the emulator was never seen"
  while read -r at kind shot type; do
    [ "$kind" = shot ] && capture "$shot" "$at" "$type"
  done < "$dir/schedule"

  if [ -n "$game" ] && kill -0 "$game" 2>/dev/null; then
    kill -TERM "$game"
    for _ in $(seq 1 20); do kill -0 "$game" 2>/dev/null || break; sleep 1; done
    kill -0 "$game" 2>/dev/null && note "quit: semu-btrc still running 20 s after SIGTERM" || note "quit: clean"
  fi
  for _ in $(seq 1 15); do kill -0 "$headless" 2>/dev/null || break; sleep 1; done
  kill -0 "$headless" 2>/dev/null && { kill -TERM "$headless"; note "headless gamescope needed a SIGTERM"; }
  kill -0 "$padpid" 2>/dev/null && kill -TERM "$padpid"
  wait "$padpid" 2>/dev/null
  injector="$(cat "$dir/inject.pid" 2>/dev/null)"
  if [ -n "$injector" ] && kill -0 "$injector" 2>/dev/null && { tr '\0' ' ' < "/proc/$injector/cmdline"; } 2>/dev/null | grep -q inject.sh; then
    kill -TERM "$injector"; note "inject: still running at the end, stopped (pid $injector)"
  fi
  evidence
  { echo "== $name"; cat "$dir/result"; } >> "$out/summary"
done < "$cases"
date > "$out/done"
