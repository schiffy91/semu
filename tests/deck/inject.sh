#!/bin/bash
# The keyboard and pointer half of an input-check.sh case on the Steam Deck. In Game Mode Steam's
# radial types its chords, and its trackpad moves the pointer, as XTest into gamescope's Xwayland,
# which hands them to gamescope through libei (LIBEI_SOCKET in Xwayland's environment, seen on the
# Deck), and gamescope gives them to the focused game. This script does the same with xdotool from
# the second Xwayland of the case's private headless gamescope (--xwayland-count 2), on the clock
# the virtual pad follows: token I fires FIRST + I*GAP seconds after SEMU_INJECT_START_MS (epoch
# milliseconds, taken when the pad started); a pad token is a pause here, as an X token is for the pad.
#
# It never types into a display it has not proven private. Its displays are the Xwaylands in its
# own mount namespace (the case's bubblewrap) whose environment carries this case's
# SEMU_INJECT_START_MS. wlroots double-forks Xwayland, so the process is a child of systemd --user,
# never of gamescope (Game Mode's :0 and :1 have that parent on the Deck). Then the display rule
# (--choose): exactly two, the game's DISPLAY one of them, both numbered 2 or higher (Game Mode's
# own are :0 and :1), both sockets present in /tmp/.X11-unix, which must be a tmpfs mounted in this
# namespace and hold no X0 or X1. Otherwise it notes "refused: why" and sends nothing at all. A
# client of :N tries the abstract socket of that number first, which wlroots binds together with
# the file socket, so a proven :N is this gamescope's.
#
#   inject.sh FIRST GAP TOKEN...                          # a case: inner.sh starts it inside the bubblewrap
#   inject.sh --plan FIRST GAP TOKEN...                   # "MS inject TOKEN: what" per X token; sends nothing
#   inject.sh --choose MOUNTINFO SOCKETS GAME DISPLAY...  # the display rule alone: the injector display or "refused: why"
#
# key:CHORD types CHORD (xdotool key --delay 80: the key is held about 40 ms); key:A,B types A, then B
# a second later, so a second press lands inside Semu's 3 s confirm window (Reset, Aspect). move:FX,FY moves the
# pointer to the fractions FX,FY of the touch screen as drawn now: parked at the top left, then moved
# relatively, as Steam's trackpad mouse moves it. tap:FX,FY moves the same way, then clicks for
# 0.15 s. The touch screen is surfaceN_content of the newest receipt in SEMU_INJECT_EVIDENCE from
# byte SEMU_INJECT_EVIDENCE_FROM on (this launch's receipts, a bezel switch's included), N the
# emulator's SEMU_RENDER_TOUCH_SURFACE_INDEX (on Wii, with no touch screen, the IR's
# SEMU_RENDER_POINTER_SURFACE_INDEX); the rectangle counts framebuffer pixels from the bottom
# left, and the framebuffer is taken to be the game's window. SEMU_INJECT_ON=game types into the
# game's own display instead, under the same rule. The rectangle is read a second before a move
# or tap is due (the emulator found with one grep over every process's environment), so the
# pointer moves on time; each note names the token's step and when it moved, pressed and released
# on the shared clock. Notes go to stdout.
set -u

fraction() {  # FX,FY: prints "FX FY" when both are numbers from 0 to 1
  printf '%s\n' "$1" | awk -F, 'NF == 2 && $1 ~ /^(0|1|0?\.[0-9]+|1\.0+)$/ && $2 ~ /^(0|1|0?\.[0-9]+|1\.0+)$/ { print $1, $2; found = 1 } END { exit !found }'
}

describe() {  # TOKEN: what an X token does (nothing for a pad token); fails on a malformed X token
  case "$1" in
    key:*) case "${1#key:}" in ''|,*|*,|*,,*|*[!A-Za-z0-9_+,]*) return 1 ;; esac  # A,B: a second press inside a confirm window
           case "${1#key:}" in
             *,*) echo "xdotool key --delay 80 $(printf '%s' "${1#key:}" | sed 's/,/, then /g'), a second apart" ;;
             *) echo "xdotool key --delay 80 ${1#key:}" ;;
           esac ;;
    move:*) fraction "${1#move:}" > /dev/null || return 1; echo "park the pointer, then mousemove_relative to ${1#move:} of the touch screen" ;;
    tap:*) fraction "${1#tap:}" > /dev/null || return 1; echo "park, mousemove_relative to ${1#tap:} of the touch screen, mousedown 1, 0.15 s, mouseup 1" ;;
    *:*) return 1 ;;  # no pad token holds a colon
  esac
  return 0
}

at_ms() {  # FIRST GAP STEP: milliseconds after the clock's zero
  awk -v first="$1" -v gap="$2" -v step="$3" 'BEGIN { printf "%d", (first + step * gap) * 1000 + 0.5 }'
}

plan() {  # FIRST GAP TOKEN...: the schedule, after checking every token
  local first="$1" gap="$2" step=0 token what
  shift 2
  case "$first$gap" in ''|*[!0-9.]*) echo "inject: FIRST and GAP must be seconds" >&2; return 64 ;; esac
  for token in "$@"; do
    what="$(describe "$token")" || { echo "inject: token $((step + 1)) '$token' is not key:CHORD, move:FX,FY or tap:FX,FY" >&2; return 2; }
    [ -n "$what" ] && echo "$(at_ms "$first" "$gap" "$step") inject $token: $what"
    step=$((step + 1))
  done
  return 0
}

choose() {  # MOUNTINFO SOCKETS GAME DISPLAY...: prints the display to type into, or "refused: why" and fails
  local mountinfo="$1" sockets="$2" game="${3%.*}" display number type other="" found=no
  shift 3
  [ "$#" -eq 2 ] || { echo "refused: the private gamescope shows $# X displays (${*:-none}), not 2"; return 1; }
  type="$(awk -v directory="$sockets" '$5 == directory { type = ""; for (field = 7; field < NF; field++) if ($field == "-") { type = $(field + 1); break } } END { print type }' "$mountinfo" 2>/dev/null)"
  [ "$type" = tmpfs ] || { echo "refused: $sockets is not a tmpfs mounted in this namespace (${type:-no mount there})"; return 1; }
  for number in 0 1; do
    [ -e "$sockets/X$number" ] && { echo "refused: $sockets holds X$number, a Game Mode display"; return 1; }
  done
  for display in "$@"; do
    number="${display#:}"; number="${number%.*}"
    case "$display" in :*) ;; *) echo "refused: $display is not a local display"; return 1 ;; esac
    case "$number" in ''|*[!0-9]*) echo "refused: $display is not a local display"; return 1 ;; esac
    [ "$number" -ge 2 ] || { echo "refused: $display is below :2 (Game Mode's own displays are :0 and :1)"; return 1; }
    [ -S "$sockets/X$number" ] || { echo "refused: no socket X$number in $sockets"; return 1; }
    if [ ":$number" = "$game" ]; then found=yes; else other=":$number"; fi
  done
  [ "$found" = yes ] || { echo "refused: the game's display ${game:-(none)} is not one of $*"; return 1; }
  [ -n "$other" ] || { echo "refused: both displays are the game's ($*)"; return 1; }
  if [ "${SEMU_INJECT_ON:-other}" = game ]; then echo "$game"; else echo "$other"; fi
}

case "${1:-}" in
  --plan) shift; plan "$@"; exit $? ;;
  --choose) shift; [ "$#" -ge 3 ] || { echo "usage: inject.sh --choose MOUNTINFO SOCKETS GAME DISPLAY..." >&2; exit 64; }; choose "$@"; exit $? ;;
esac
[ "$#" -ge 2 ] || { echo "usage: inject.sh FIRST GAP TOKEN... | --plan FIRST GAP TOKEN... | --choose MOUNTINFO SOCKETS GAME DISPLAY..." >&2; exit 64; }
first="$1"; gap="$2"; shift 2
plan "$first" "$gap" "$@" > /dev/null || exit $?  # a malformed case sends nothing
start_ms="${SEMU_INJECT_START_MS:-}"
case "$start_ms" in ''|*[!0-9]*) echo "inject: refused: SEMU_INJECT_START_MS is not set; nothing sent"; exit 64 ;; esac
own() {  # PID: is this process in this case (this mount namespace, this case's clock)?
  [ "/proc/$1/ns/mnt" -ef /proc/self/ns/mnt ] || return 1  # the same namespace inode, without a fork per process
  { tr '\0' '\n' < "/proc/$1/environ"; } 2>/dev/null | grep -q -x "SEMU_INJECT_START_MS=$start_ms"
}

emulator_environ() {  # the environ file of this case's emulator. It runs inside Semu's own bubblewrap, a nested mount
  # namespace, so only the inherited SEMU_INJECT_START_MS tells it belongs to this case: one grep over every process's
  # environment finds the few that carry it (a fork per process took a second on the Deck), and the emulator is the one
  # Semu gave a render state directory
  local carriers
  carriers="$(grep -l -z -x "SEMU_INJECT_START_MS=$start_ms" /proc/[0-9]*/environ 2>/dev/null)"
  [ -n "$carriers" ] || return 1
  printf '%s\n' "$carriers" | while IFS= read -r file; do
    grep -q -z '^SEMU_RENDER_STATE_DIR=' "$file" 2>/dev/null && { echo "$file"; break; }
  done
}

displays() {  # this case's Xwaylands, by the display each serves (argv 1)
  local pid
  for pid in $(pgrep -x Xwayland); do
    own "$pid" && { tr '\0' '\n' < "/proc/$pid/cmdline"; } 2>/dev/null | sed -n '2p'
  done | sort -u
}

elapsed() { awk -v now="$(date +%s%3N)" -v zero="$start_ms" 'BEGIN { printf "%.3f", (now - zero) / 1000 }'; }
wait_for() {  # MS on the shared clock
  local left=$(( start_ms + $1 - $(date +%s%3N) ))
  [ "$left" -gt 0 ] && sleep "$(awk -v left="$left" 'BEGIN { printf "%.3f", left / 1000 }')"
  return 0
}

touch_rect() {  # prints "LEFT TOP WIDTH HEIGHT FRAME_W FRAME_H SURFACE" for the touch screen as drawn now, or a reason and fails
  local file environment="" index state line
  file="$(emulator_environ)"
  [ -n "$file" ] && environment="$({ tr '\0' '\n' < "$file"; } 2>/dev/null)"
  [ -n "$environment" ] || { echo "no emulator of this case is running"; return 1; }
  index="$(printf '%s\n' "$environment" | sed -n 's/^SEMU_RENDER_TOUCH_SURFACE_INDEX=//p')"
  [ -n "$index" ] || index="$(printf '%s\n' "$environment" | sed -n 's/^SEMU_RENDER_POINTER_SURFACE_INDEX=//p')"  # Wii: the screen the IR aims at
  state="$(printf '%s\n' "$environment" | sed -n 's/^SEMU_RENDER_STATE_DIR=//p')"
  [ -n "$index" ] || { echo "the emulator has no touch or pointer surface (no SEMU_RENDER_TOUCH_SURFACE_INDEX or SEMU_RENDER_POINTER_SURFACE_INDEX)"; return 1; }
  [ "$state/semu-render-evidence.log" = "${SEMU_INJECT_EVIDENCE:-}" ] || { echo "the emulator writes $state/semu-render-evidence.log, not ${SEMU_INJECT_EVIDENCE:-(none)}"; return 1; }
  line="$(tail -c +"${SEMU_INJECT_EVIDENCE_FROM:-1}" "$SEMU_INJECT_EVIDENCE" 2>/dev/null | grep "surface${index}_content=" | tail -1)"
  [ -n "$line" ] || { echo "no receipt of this launch draws surface $index yet"; return 1; }
  printf '%s\n' "$line" | awk -v key="surface${index}_content" -v surface="$index" '{
      for (field = 1; field <= NF; field++) { split($field, pair, "="); value[pair[1]] = pair[2] }
      split(value[key], rect, ","); split(value["framebuffer_size"], frame, "x")
      if (rect[3] <= 0 || rect[4] <= 0 || frame[2] <= 0) exit 1
      print rect[1], frame[2] - rect[2] - rect[4], rect[3], rect[4], frame[1], frame[2], surface }' || { echo "the receipt has no usable surface $index rectangle"; return 1; }
}

point() {  # FX FY LEFT TOP WIDTH HEIGHT: the pixel inside the rectangle at those fractions
  awk -v across="$1" -v down="$2" -v left="$3" -v top="$4" -v width="$5" -v height="$6" 'BEGIN {
      column = left + int(across * width); row = top + int(down * height)
      if (column > left + width - 1) column = left + width - 1
      if (row > top + height - 1) row = top + height - 1
      print column, row }'
}

found=""
for _ in $(seq 1 40); do  # both Xwaylands are up before gamescope starts inner.sh; allow 20 s anyway
  found="$(displays)"
  [ "$(printf '%s\n' "$found" | grep -c .)" -ge 2 ] && break
  sleep 0.5
done
injector="$(choose /proc/self/mountinfo /tmp/.X11-unix "${DISPLAY:-}" $found)" || { echo "inject: $injector; nothing sent"; exit 3; }
echo "inject: typing into $injector (the game's display ${DISPLAY:-}; this case's displays: $(echo $found))"
send() { DISPLAY="$injector" xdotool "$@"; }  # XTest into the proven display only

step=0
for token in "$@"; do
  what="$(describe "$token")"
  if [ -n "$what" ]; then
    due="$(at_ms "$first" "$gap" "$step")"
    resolved=no
    case "$token" in
      move:*|tap:*)  # the touch screen is found a second ahead, so the pointer moves on time
        wait_for "$((due - 1000))"
        rect="$(touch_rect)" && resolved=yes ;;
    esac
    wait_for "$due"
    began="$(elapsed)"; when="$(awk -v due="$due" 'BEGIN { printf "%.3f", due / 1000 }') (began $began)"
    case "$token" in
      key:*)  # A,B types A, then B a second later
        sent=0; rest="${token#key:}"
        while :; do
          chord="${rest%%,*}"
          send key --delay 80 "$chord" || sent=$?
          [ "$chord" = "$rest" ] && break
          rest="${rest#*,}"; sleep 1
        done
        echo "inject: $when $token sent to $injector (xdotool $sent)" ;;
      move:*|tap:*)
        [ "$resolved" = yes ] || { rect="$(touch_rect)" && resolved=yes; }  # no receipt a second ago: one more look
        if [ "$resolved" = yes ]; then
          read -r left top width height frame_width frame_height surface <<< "$rect"
          read -r across down <<< "$(fraction "${token#*:}")"
          read -r column row <<< "$(point "$across" "$down" "$left" "$top" "$width" "$height")"
          send mousemove_relative -- -4000 -4000; sleep 0.05
          send mousemove_relative -- "$column" "$row"
          moved=$?; times="moved at $(elapsed)"
          if [ "${token%%:*}" = tap ]; then
            sleep 0.1; send mousedown 1; times="$times, down at $(elapsed)"
            sleep 0.15; send mouseup 1; times="$times, up at $(elapsed)"
          fi
          echo "inject: $when step $((step + 1)) $token -> $column,$row (surface $surface at $left,$top ${width}x$height in ${frame_width}x$frame_height) sent to $injector (xdotool $moved), $times"
        else
          echo "inject: $when step $((step + 1)) $token skipped: $rect"
        fi ;;
    esac
  fi
  step=$((step + 1))
done
echo "inject: done at $(elapsed)"
