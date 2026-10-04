#!/bin/bash
# Every system on the Steam Deck, off-screen. Each case runs the installed release the way ES-DE
# does (semu-deck-cli launch), inside a private headless gamescope at the Deck's 1280x800 with the
# sound cut (SDL gets its silent dummy driver: Ryujinx refuses to start without an audio device),
# so the Deck's own screen, Steam and Game Mode are left alone. A case is captured at each of its
# waits (seconds after launch), with the emulator's state and bytes read, and then quit the way
# Semu quits: SIGTERM to semu-btrc, which ends the emulator's whole process group. Everything is
# tracked by PID. The script writes only below OUT and removes nothing.
#
#   system-matrix.sh CASES OUT
#
# CASES has one case per line: SYSTEM EMULATOR CORE|- WAIT... -- ROM-GLOB (relative to the ROM
# folder of SYSTEM; the first match is played). OUT/<case>/ gets at-<wait>.png for each wait,
# run.log (everything Semu and the emulator printed), render-env (the SEMU_RENDER_* the emulator
# saw), cmdline (its argv, one argument per line, read the moment the emulator is first seen, so an
# emulator that exits at once still shows which file it was given) and result (one line per check);
# a second case of the same SYSTEM-EMULATOR[-CORE] gets a -2, -3 suffix. OUT/summary collects every
# result; OUT/done marks the end. The result names the second the first composed frame was logged,
# so a run right after a release and a second run give the first and second boot times.
#
#   SEMU_MATRIX_SETTINGS='{"visual":{"performance_overlay":true}}' system-matrix.sh CASES OUT
#
# passes that JSON as every launch's --settings-json, the highest settings overlay, so the owner's
# semu.json is never written. With the performance overlay on, Dolphin logs one line per frame and
# per VI; a Dolphin case copies Logs/render_times.txt and vblank_times.txt beside its result and
# notes their rates (VIs per second against 59.94 or 50 is the emulation speed; presents undercount,
# as Dolphin skips duplicate frames).
set -u
cases="$1"; out="$2"
roms=/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
cli="$HOME/Applications/Semu/bin/semu-deck-cli"
state="$HOME/.local/share/semu"  # the steam-deck target's state_root
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
emulators='retroarch|azahar|dolphin|pcsx2|ppsspp|melonds|flycast|cemu|ryujinx|semu-btrc'
mkdir -p "$out"
printf "pcm.!default {\n  type null\n}\nctl.!default {\n  type hw\n  card 0\n}\n" > "$out/alsa-null.conf"  # cubeb (PCSX2) falls back to ALSA: a silent device, or a modal error dialog hides the game

descendants() { local child; for child in $(pgrep -P "$1"); do echo "$child"; descendants "$child"; done; }
battery() { cat /sys/class/power_supply/BAT1/capacity 2>/dev/null || echo 100; }
charging() { grep -q -E 'Charging|Full' /sys/class/power_supply/BAT1/status 2>/dev/null; }
emulator_pids() { pgrep -f "/($emulators)([^/]*)( |$)" 2>/dev/null | sort; }
leaf_of() { local leaf="$1" child; while [ -n "$leaf" ] && child=$(pgrep -P "$leaf" | tail -1) && [ -n "$child" ]; do leaf=$child; done; echo "$leaf"; }
record_argv() {  # $1 the emulator: its argv once it has exec'd, silently nothing if it is already gone
  local leaf="$1"
  if [ -s "$dir/cmdline" ] || [ -z "$leaf" ] || [ "$leaf" = "$game" ] || [ "$(cat "/proc/$leaf/comm" 2>/dev/null)" = semu-btrc ]; then return 0; fi
  { tr '\0' '\n' < "/proc/$leaf/cmdline"; } > "$dir/cmdline" 2>/dev/null
  [ -s "$dir/cmdline" ] && note "argv-last: $(tail -1 "$dir/cmdline")"  # the file the emulator was told to open
  return 0
}

before="$(emulator_pids)"
while IFS= read -r line; do
  case "$line" in ''|'#'*) continue ;; esac
  head="${line%% -- *}"; pattern="${line#* -- }"
  read -r system emulator core waits <<< "$head"
  name="$system-$emulator"; [ "$core" = - ] || name="$name-$core"
  base="$name"; suffix=2; while [ -e "$out/$name" ]; do name="$base-$suffix"; suffix=$((suffix + 1)); done  # a second case of one emulator keeps its own results
  dir="$out/$name"; mkdir -p "$dir"; : > "$dir/result"
  note() { echo "$*" >> "$dir/result"; }
  if [ "$(battery)" -lt 20 ] && ! charging; then note "skipped: battery $(battery)% and not charging"; continue; fi
  for pid in $(pgrep -f "/(es-de|$emulators)([^/]*)( |$)" 2>/dev/null); do  # the owner is playing: leave the Deck to them
    tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null | grep -q '^SEMU_MATRIX_ROM=' && continue
    note "stopped: the owner's $(cat "/proc/$pid/comm" 2>/dev/null) (pid $pid) is running"; { echo "== $name"; cat "$dir/result"; } >> "$out/summary"; date > "$out/done"; exit 0
  done
  rom="$(compgen -G "$roms/$system/$pattern" | head -1)"
  if [ -z "$rom" ]; then note "skipped: no ROM matches $system/$pattern"; continue; fi
  note "rom: ${rom#"$roms/"}"
  core_argument=""; [ "$core" = - ] || core_argument="--core $core"
  printf '#!/bin/sh\nexec "%s" launch %s --system %s %s ${SEMU_MATRIX_SETTINGS:+--settings-json "$SEMU_MATRIX_SETTINGS"} --rom "$SEMU_MATRIX_ROM"\n' "$cli" "$emulator" "$system" "$core_argument" > "$dir/inner.sh"
  chmod +x "$dir/inner.sh"

  start=$(date +%s)
  PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy ALSA_CONFIG_PATH="$out/alsa-null.conf" SEMU_RENDER_DEBUG=1 SEMU_MATRIX_ROM="$rom" \
    gamescope --backend headless -W 1280 -H 800 -w 1280 -h 800 -- "$dir/inner.sh" > "$dir/run.log" 2>&1 &
  headless=$!
  game=""; display=""
  for _ in $(seq 1 300); do  # every 0.2 s for 60 s: an emulator that exits within a second is still seen
    for pid in $(descendants "$headless"); do
      [ -z "$display" ] && display="$(tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null | sed -n 's/^GAMESCOPE_WAYLAND_DISPLAY=//p')"
      [ "$(cat "/proc/$pid/comm" 2>/dev/null)" = semu-btrc ] && game=$pid
    done
    [ -n "$game" ] && break
    kill -0 "$headless" 2>/dev/null || break
    sleep 0.2
  done
  [ -n "$game" ] && note "semu-btrc: pid $game after $(( $(date +%s) - start )) s" || note "semu-btrc: never started"
  for _ in $(seq 1 100); do  # the emulator's argv the moment it appears, before any wait
    [ -n "$game" ] || break
    record_argv "$(leaf_of "$game")"
    [ -s "$dir/cmdline" ] && break
    kill -0 "$game" 2>/dev/null || break
    sleep 0.1
  done

  first=""
  for wait in $waits; do
    while [ $(( $(date +%s) - start )) -lt "$wait" ]; do
      if [ -z "$first" ] && grep -q 'semu-compose: first frame' "$dir/run.log" 2>/dev/null; then first=$(( $(date +%s) - start )); note "first frame: after $first s"; fi
      sleep 1
    done
    running=no; [ -n "$game" ] && kill -0 "$game" 2>/dev/null && running=yes
    shot="$dir/at-$wait.png"
    [ -n "$display" ] && GAMESCOPE_WAYLAND_DISPLAY="$display" timeout 20 gamescopectl screenshot "$shot" > /dev/null 2>&1
    for _ in $(seq 1 10); do [ -s "$shot" ] && break; sleep 1; done
    leaf="$(leaf_of "$game")"; record_argv "$leaf"
    io=""; [ -n "$leaf" ] && io="$(cat "/proc/$leaf/comm" 2>/dev/null) state $(awk '{print $3}' "/proc/$leaf/stat" 2>/dev/null), read $(( $(sed -n 's/^rchar: //p' "/proc/$leaf/io" 2>/dev/null || echo 0) / 1048576 )) MB, cpu $(ps -o pcpu= -p "$leaf" 2>/dev/null | tr -d ' ')%"
    note "t=$wait running=$running shot=$([ -s "$shot" ] && echo yes || echo no) $io"
  done
  leaf="$(leaf_of "$game")"; record_argv "$leaf"
  [ -s "$dir/cmdline" ] || note "argv: the emulator was never seen"
  [ -n "$leaf" ] && { tr '\0' '\n' < "/proc/$leaf/environ" | grep '^SEMU_RENDER_' | sort; } > "$dir/render-env" 2>/dev/null
  [ -n "$leaf" ] && note "emulator: $(cat "/proc/$leaf/comm" 2>/dev/null) pid $leaf, $(ps -o pcpu=,rss= -p "$leaf" 2>/dev/null | awk '{printf "%s%% cpu, %d MB", $1, $2/1024}')"

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
    tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null | grep -q '^SEMU_MATRIX_ROM=' && orphans="$orphans $pid"
  done
  if [ -n "$orphans" ]; then
    note "cleanup: orphans $(for pid in $orphans; do printf '%s(%s) ' "$pid" "$(cat "/proc/$pid/comm" 2>/dev/null)"; done)"
    for pid in $orphans; do kill -TERM "$pid" 2>/dev/null; done; sleep 2
    for pid in $orphans; do kill -0 "$pid" 2>/dev/null && kill -KILL "$pid" 2>/dev/null; done
  fi
  if [ "$emulator" = dolphin ]; then  # Dolphin's frame logs, written only while the performance overlay is on
    for log in render_times vblank_times; do
      file="$state/dolphin/dolphin-user/Logs/$log.txt"
      [ "$file" -nt "$dir/inner.sh" ] || continue
      cp "$file" "$dir/$log.txt"
      note "$log: $(awk '{ value[NR] = $1; total += $1 } END { for (line = int(NR / 2) + 1; line <= NR; line++) { late += value[line]; count++ } if (total > 0 && late > 0) printf "%d lines, %.2f per second overall, %.2f per second in the second half", NR, 1000 * NR / total, 1000 * count / late }' "$file")"
    done
  fi
  grep -E 'semu-compose: first frame|semu:|error|Error|fatal|Fatal' "$dir/run.log" | grep -v -i 'fontconfig' | head -12 | sed 's/^/log: /' >> "$dir/result"
  { echo "== $name"; cat "$dir/result"; } >> "$out/summary"
  sleep 3
done < "$cases"
date > "$out/done"
