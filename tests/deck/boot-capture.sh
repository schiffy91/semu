#!/bin/bash
# The first seconds of a launch on the Steam Deck, off-screen, as a film: each case runs the installed release
# the way ES-DE does (semu-deck-cli launch), inside a private headless gamescope at the Deck's 1280x800 with the
# sound cut, as system-matrix.sh does, and asks that gamescope for a screenshot every INTERVAL seconds (default
# 0.25, SEMU_BOOT_INTERVAL) from the launch to SECONDS, without waiting for each to be written. Beside each shot
# it notes the X windows gamescope's Xwayland holds (xwininfo, when the Deck has it), and the launch runs with
# SEMU_RENDER_DEBUG=1, so the renderer's "semu-renderer: framebuffer WxH" lines say what size Semu composed into and
# when. That is what the owner's "correctly sized, then a third of the screen in the top left, then right again"
# boot looks like from the inside (M16 item 4). Quits the way Semu quits (SIGTERM to semu-btrc), tracks
# everything by PID, writes only below OUT and removes nothing.
#
#   boot-capture.sh CASES OUT               # on the Deck
#   boot-capture.sh --analyse OUT           # where ImageMagick is (the Mac, after fetching OUT): the timeline
#
# CASES has one case per line: SYSTEM EMULATOR CORE|- SECONDS -- ROM-GLOB (relative to SYSTEM's ROM folder; the
# first match is played). OUT/<case>/ gets shot-<ms>.png (milliseconds after launch), windows.log (t=<ms> then
# xwininfo's tree), run.log, cmdline, and result (the framebuffer lines at their t=, the composed frame rate
# lines). --analyse writes OUT/<case>/timeline: each shot's lit box (the rectangle holding every pixel brighter
# than near-black, from ImageMagick's trim) and SHRUNK where that box hugs the top left and leaves at least a
# fifth of the screen black on the right and at the bottom, the owner's symptom; OUT/<case>/sheet.png puts
# the shots side by side, and OUT/analysis sums the cases up. Judge the sheets by eye: a title screen
# can be small on purpose.
set -u
if [ "${1:-}" = --analyse ]; then
  out="${2:?usage: boot-capture.sh --analyse OUT}"
  command -v magick > /dev/null || { echo "boot-capture: --analyse needs ImageMagick (magick)" >&2; exit 2; }
  : > "$out/analysis"
  for dir in "$out"/*/; do
    dir="${dir%/}"; name="${dir##*/}"
    ls "$dir"/shot-*.png > /dev/null 2>&1 || continue
    : > "$dir/timeline"; shrunk=0; shots=0; first=""; last=""
    for shot in "$dir"/shot-*.png; do
      at="${shot##*/shot-}"; at="${at%.png}"
      read -r width height < <(magick identify -format '%w %h\n' "$shot" 2>/dev/null | head -1)
      [ -n "${width:-}" ] || continue
      box="$(magick "$shot" -alpha off -fuzz 6% -bordercolor black -border 1 -trim -format '%w %h %X %Y' info: 2>/dev/null)"
      read -r boxWidth boxHeight boxX boxY <<< "$box"
      boxX="${boxX#+}"; boxY="${boxY#+}"; boxX=$((boxX - 1)); boxY=$((boxY - 1))  # the 1-px border shifts the page offset
      verdict=""
      if [ "$boxWidth" -le 2 ] && [ "$boxHeight" -le 2 ]; then verdict="black"
      elif [ "$boxX" -le $((width / 20)) ] && [ "$boxY" -le $((height / 20)) ] && [ $((boxX + boxWidth)) -le $((width * 4 / 5)) ] && [ $((boxY + boxHeight)) -le $((height * 4 / 5)) ]; then
        verdict="SHRUNK"; shrunk=$((shrunk + 1)); [ -n "$first" ] || first="$at"; last="$at"
      fi
      shots=$((shots + 1))
      printf 't=%6.2f lit %4dx%-4d at %4d,%-4d %s\n' "$(awk -v at="$at" 'BEGIN { print at / 1000 }')" "$boxWidth" "$boxHeight" "$boxX" "$boxY" "$verdict" >> "$dir/timeline"
    done
    magick montage "$dir"/shot-*.png -tile 8x -geometry 320x200+2+2 -background '#333' "$dir/sheet.png" 2>/dev/null
    if [ "$shrunk" -gt 0 ]; then
      echo "$name: $shrunk of $shots shots SHRUNK to the top left, t=$(awk -v at="$first" 'BEGIN { print at / 1000 }') to $(awk -v at="$last" 'BEGIN { print at / 1000 }') s" >> "$out/analysis"
    else
      echo "$name: $shots shots, none shrunk to the top left" >> "$out/analysis"
    fi
  done
  cat "$out/analysis"
  exit 0
fi

cases="$1"; out="$(mkdir -p "$2" && cd "$2" && pwd -P)"
interval="${SEMU_BOOT_INTERVAL:-0.25}"
roms=/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
cli="$HOME/Applications/Semu/bin/semu-deck-cli"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
emulators='retroarch|azahar|dolphin|pcsx2|ppsspp|melonds|flycast|cemu|ryujinx|semu-btrc'
printf "pcm.!default {\n  type null\n}\nctl.!default {\n  type hw\n  card 0\n}\n" > "$out/alsa-null.conf"

descendants() { local child; for child in $(pgrep -P "$1"); do echo "$child"; descendants "$child"; done; }
battery() { cat /sys/class/power_supply/BAT1/capacity 2>/dev/null || echo 100; }
charging() { grep -q -E 'Charging|Full' /sys/class/power_supply/BAT1/status 2>/dev/null; }
emulator_pids() { pgrep -f "/($emulators)([^/]*)( |$)" 2>/dev/null | sort; }
leaf_of() { local leaf="$1" child; while [ -n "$leaf" ] && child=$(pgrep -P "$leaf" | tail -1) && [ -n "$child" ]; do leaf=$child; done; echo "$leaf"; }
elapsed_ms() { echo $(( $(date +%s%3N) - start_ms )); }

before="$(emulator_pids)"
while IFS= read -r line; do
  case "$line" in ''|'#'*) continue ;; esac
  head="${line%% -- *}"; pattern="${line#* -- }"
  read -r system emulator core seconds <<< "$head"
  name="$system-$emulator"; [ "$core" = - ] || name="$name-$core"
  base="$name"; suffix=2; while [ -e "$out/$name" ]; do name="$base-$suffix"; suffix=$((suffix + 1)); done
  dir="$out/$name"; mkdir -p "$dir"; : > "$dir/result"
  note() { echo "$*" >> "$dir/result"; }
  if [ "$(battery)" -lt 25 ] && ! charging; then note "skipped: battery $(battery)% and not charging"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; continue; fi
  for pid in $(pgrep -f "/(es-de|$emulators)([^/]*)( |$)" 2>/dev/null); do  # the owner is playing: leave the Deck to them
    { tr '\0' '\n' < "/proc/$pid/environ"; } 2>/dev/null | grep -q '^SEMU_MATRIX_ROM=' && continue
    note "stopped: the owner's $(cat "/proc/$pid/comm" 2>/dev/null) (pid $pid) is running"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; date > "$out/done"; exit 0
  done
  rom="$(compgen -G "$roms/$system/$pattern" | head -1)"
  if [ -z "$rom" ]; then note "skipped: no ROM matches $system/$pattern"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; continue; fi
  note "rom: ${rom#"$roms/"} (battery $(battery)%)"
  core_argument=""; [ "$core" = - ] || core_argument="--core $core"
  printf '#!/bin/sh\nexec "%s" launch %s --system %s %s ${SEMU_MATRIX_SETTINGS:+--settings-json "$SEMU_MATRIX_SETTINGS"} --rom "$SEMU_MATRIX_ROM"\n' "$cli" "$emulator" "$system" "$core_argument" > "$dir/inner.sh"
  chmod +x "$dir/inner.sh"

  start_ms=$(date +%s%3N)
  PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy ALSA_CONFIG_PATH="$out/alsa-null.conf" SEMU_RENDER_DEBUG=1 SEMU_MATRIX_ROM="$rom" \
    gamescope --backend headless -W 1280 -H 800 -w 1280 -h 800 -- "$dir/inner.sh" > "$dir/run.log" 2>&1 &
  headless=$!
  game=""; display=""; xdisplay=""
  while [ "$(elapsed_ms)" -lt $(( seconds * 1000 )) ]; do
    tick=$(elapsed_ms)
    if [ -z "$display" ] || [ -z "$game" ] || [ -z "$xdisplay" ]; then
      for pid in $(descendants "$headless"); do
        environment="$({ tr '\0' '\n' < "/proc/$pid/environ"; } 2>/dev/null)"
        [ -z "$display" ] && display="$(printf '%s\n' "$environment" | sed -n 's/^GAMESCOPE_WAYLAND_DISPLAY=//p')"
        [ -z "$xdisplay" ] && xdisplay="$(printf '%s\n' "$environment" | sed -n 's/^DISPLAY=//p')"
        [ -z "$game" ] && [ "$(cat "/proc/$pid/comm" 2>/dev/null)" = semu-btrc ] && { game=$pid; note "semu-btrc: pid $game at t=$(awk -v at="$tick" 'BEGIN { print at / 1000 }')"; }
      done
    fi
    if [ -n "$display" ]; then
      ( GAMESCOPE_WAYLAND_DISPLAY="$display" timeout 5 gamescopectl screenshot "$dir/shot-$(printf '%05d' "$tick").png" > /dev/null 2>&1 & )
    fi
    if [ -n "$xdisplay" ] && command -v xwininfo > /dev/null; then
      { echo "t=$tick"; DISPLAY="$xdisplay" timeout 1 xwininfo -root -tree 2>/dev/null | grep -E '^ +0x[0-9a-f]+ ' | grep -v -E ' 1x1\+| 10x10\+'; } >> "$dir/windows.log"
    fi
    kill -0 "$headless" 2>/dev/null || { note "the private gamescope exited at t=$(awk -v at="$tick" 'BEGIN { print at / 1000 }')"; break; }
    left=$(( start_ms + tick + $(awk -v interval="$interval" 'BEGIN { printf "%d", interval * 1000 }') - $(date +%s%3N) ))
    [ "$left" -gt 0 ] && sleep "$(awk -v left="$left" 'BEGIN { printf "%.3f", left / 1000 }')"
  done
  sleep 2  # the last screenshots finish writing
  leaf="$(leaf_of "$game")"
  [ -n "$leaf" ] && [ "$leaf" != "$game" ] && { tr '\0' '\n' < "/proc/$leaf/cmdline"; } > "$dir/cmdline" 2>/dev/null
  note "shots: $(ls "$dir"/shot-*.png 2>/dev/null | wc -l) written, every $interval s to $seconds s"
  grep -a -o 'semu-renderer: framebuffer [0-9]*x[0-9]* (was [0-9]*x[0-9]*) at frame [0-9]* ms=[0-9]*' "$dir/run.log" \
    | awk -v zero="$start_ms" '{ split($NF, pair, "="); $NF = ""; printf "%s t=%.3f\n", $0, (pair[2] - zero) / 1000 }' >> "$dir/result"
  grep -a -o 'semu-renderer: [0-9]* frames in .*' "$dir/run.log" | head -12 >> "$dir/result"
  grep -a -o 'semu-compose: first frame .*' "$dir/run.log" | head -1 >> "$dir/result"

  if [ -n "$game" ] && kill -0 "$game" 2>/dev/null; then
    kill -TERM "$game"
    for _ in $(seq 1 40); do kill -0 "$game" 2>/dev/null || break; sleep 0.5; done
    kill -0 "$game" 2>/dev/null && note "quit: semu-btrc still running 20 s after SIGTERM" || note "quit: clean"
  else
    note "quit: semu-btrc had already exited"
  fi
  for _ in $(seq 1 20); do kill -0 "$headless" 2>/dev/null || break; sleep 0.5; done
  if kill -0 "$headless" 2>/dev/null; then  # the private gamescope outlived its game: stop what is left of this case
    for pid in $(descendants "$headless"); do kill -TERM "$pid" 2>/dev/null; done
    kill -TERM "$headless" 2>/dev/null; sleep 3
    for pid in $(descendants "$headless") "$headless"; do kill -0 "$pid" 2>/dev/null && kill -KILL "$pid" 2>/dev/null; done
    note "cleanup: the private gamescope had to be stopped"
  fi
  orphans=""  # started by this case (they carry its SEMU_MATRIX_ROM) and outlived it; anyone else's are left alone
  for pid in $(comm -13 <(echo "$before") <(emulator_pids)); do
    { tr '\0' '\n' < "/proc/$pid/environ"; } 2>/dev/null | grep -q '^SEMU_MATRIX_ROM=' && orphans="$orphans $pid"
  done
  if [ -n "$orphans" ]; then
    note "cleanup: orphans $(for pid in $orphans; do printf '%s(%s) ' "$pid" "$(cat "/proc/$pid/comm" 2>/dev/null)"; done)"
    for pid in $orphans; do kill -TERM "$pid" 2>/dev/null; done; sleep 2
    for pid in $orphans; do kill -0 "$pid" 2>/dev/null && kill -KILL "$pid" 2>/dev/null; done
  fi
  { echo "== $name"; cat "$dir/result"; } >> "$out/summary"
  sleep 3
done < "$cases"
date > "$out/done"
