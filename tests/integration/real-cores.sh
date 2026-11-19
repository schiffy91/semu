#!/bin/sh
# One real core with a real (freely licensed) test program through `semu launch`: it boots, draws a frame
# that is not blank, saves a state file, loads it back, loads the state the same program left under the
# lock before this one, and quits through RetroArch's QUIT. No step waits a fixed time: each waits for the
# thing itself, so a busy machine is slower and not wrong, and a core that dies is reported as having
# died, with how.
# Inputs: SEMU_CLI, RETROARCH, RENDERER (renderer lib dir), SYSTEM, CORE, ROM, COREFILE.
# Optional: PREVIOUS_STATE (a state file saved under an earlier lock), STATE_OUT (where this run's goes).
set -eu
work="${TMPDIR:-/tmp}/semu-real-core"
command_port() { printf '%s' "$1" | socat -t 1 - UDP:127.0.0.1:55355 2>/dev/null | tr -d '\n'; }
fail() {
  echo "$SYSTEM ($CORE): $1"
  tail -12 "$work/launch.log" 2>/dev/null | cut -c1-240 || true
  holding
  kill -TERM "$launcher" 2>/dev/null || true
  exit 1
}
# What each RetroArch still there is doing and holding: its threads (name, state, user and system ticks)
# and the mappings with the most resident memory. A RetroArch that grows without bound has done so once in
# some hundreds of runs, on a loaded machine, and this is what says where when it does so again.
holding() {
  for status in /proc/[0-9]*/status; do
    case "$(sed -n 1p "$status" 2>/dev/null)" in *retroarch*) ;; *) continue ;; esac
    process="${status%/status}"
    echo "$process: $(grep -E '^(VmRSS|RssAnon|RssFile|RssShmem|Threads)' "$status" 2>/dev/null | tr -s '\t \n' ' ')"
    for task in "$process"/task/*; do
      echo "  thread $(cat "$task/comm" 2>/dev/null): $(awk '{ print $3, $14, $15 }' "$task/stat" 2>/dev/null)"
    done
    awk '/^[0-9a-f]+-[0-9a-f]+ / { name = $6 == "" ? "anonymous " $1 : $6 } /^Rss:/ { held[name] += $2 }
      END { for (name in held) print held[name], name }' "$process/smaps" 2>/dev/null | sort -rn | head -6 | sed 's/^/  kB /'
  done
}
gone() {  # RetroArch is no longer there: say how it left
  code=0
  wait "$launcher" || code=$?
  fail "RetroArch left with status $code$([ "$code" -gt 128 ] && echo " (signal $((code - 128)))") while waiting for $1"
}
# RetroArch's resident memory in MiB, 0 when it is not there. On 19 November one grew to 52 GB while this
# check waited for its first answer, and the kernel killed it; nothing here said why it was gone.
resident() {
  total=0
  for status in /proc/[0-9]*/status; do
    case "$(sed -n 1p "$status" 2>/dev/null)" in *retroarch*) ;; *) continue ;; esac
    kilobytes="$(sed -n 's/^VmRSS:[[:space:]]*\([0-9]*\).*/\1/p' "$status" 2>/dev/null)"
    [ -z "$kilobytes" ] || total=$((total + kilobytes / 1024))
  done
  echo "$total"
}
# await SECONDS WHAT COMMAND...: the command is tried four times a second until it holds.
await() {
  tries=$(( $1 * 4 )); what=$2; shift 2
  while ! "$@"; do
    kill -0 "$launcher" 2>/dev/null || gone "$what"
    grown="$(resident)"
    [ "$grown" -lt 3072 ] || fail "RetroArch holds $grown MiB while waiting for $what: it is growing without bound"
    tries=$((tries - 1))
    [ "$tries" -gt 0 ] || fail "no $what in time"
    sleep 0.25
  done
}
answers() { [ -n "$(printf VERSION | socat -t 0.2 - UDP:127.0.0.1:55355 2>/dev/null || true)" ]; }  # a short wait: this one is timed
drawn() {  # a screenshot taken now is not one flat colour
  rm -f "$work/content/screenshots/"*.png
  command_port SCREENSHOT > /dev/null || true
  shot=""
  for _ in 1 2 3 4 5 6 7 8; do
    shot="$(find "$work/content/screenshots" -name '*.png' 2>/dev/null | head -1)"
    [ -z "$shot" ] || break
    sleep 0.25
  done
  [ -n "$shot" ] || return 1
  spread="$(magick identify -format '%[fx:standard_deviation]' "$shot" 2>/dev/null || echo 0)"
  awk "BEGIN { exit !($spread > 0.02) }"
}
saved() {  # asked again each second: ppsspp writes no state for a request that comes too early
  state="$(find "$work/content/states" -name '*.state*' 2>/dev/null | head -1)"
  [ -z "$state" ] || return 0
  asked=$((asked + 1))
  [ $((asked % 4)) -ne 1 ] || command_port SAVE_STATE > /dev/null || true
  return 1
}

# RetroArch has taken up the load asked for after line $mark of its log: it keeps a copy of the running
# state in memory just before it hands the file to the core, and says so.
taken() { tail -n +$((mark + 1)) "$work/launch.log" | grep -q -e 'Saving state "RAM"' -e 'Failed to load state'; }
load() {  # load WHAT: the state file is loaded, and a core that refuses it fails the check
  mark=$(wc -l < "$work/launch.log")
  command_port LOAD_STATE > /dev/null || true
  await 45 "RetroArch to take up $1" taken
  await 45 "an answer after loading $1" answers
  ! tail -n +$((mark + 1)) "$work/launch.log" | grep -q 'Failed to load state' || fail "the core refused $1"
}

config="$(dirname "$(readlink -f "$SEMU_CLI")")/../share/semu/config"
rm -rf "$work"
mkdir -p "$work/home" "$work/assets/bin" "$work/assets/lib/retroarch/cores"
export HOME="$work/home" LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
unset WAYLAND_DISPLAY
# RetroArch says whether a state loaded only in its verbose log, and the launcher has no word for that:
# this asset folder is the check's own, so the RetroArch in it is a line that adds the flag.
printf '#!/bin/sh\nexec "%s" --verbose "$@"\n' "$RETROARCH" > "$work/assets/bin/retroarch"
chmod +x "$work/assets/bin/retroarch"
ln -s "$RENDERER/libsemurenderer.so" "$work/assets/lib/libsemurenderer.so"
ln -s "$COREFILE" "$work/assets/lib/retroarch/cores/${CORE}_libretro.so"
settings="{\"paths\":{\"roms\":\"$work/roms\",\"state_root\":\"$work/state\",\"content_root\":\"$work/content\"}}"
romDirectory="$work/roms/$(jq -r '.rom.dir' "$config/systems/$SYSTEM/system.json")"
mkdir -p "$romDirectory"
cp "$ROM" "$romDirectory/"
name="$(basename "$ROM")"
"$SEMU_CLI" launch retroarch --system "$SYSTEM" --rom "$name" --asset-root "$work/assets" --settings-json "$settings" > "$work/launch.log" 2>&1 &
launcher=$!
started=$(date +%s%N)
asked=0
await 90 "an answer from RetroArch" answers
answered=$(( ($(date +%s%N) - started) / 1000000 ))
await 90 "a frame that is not blank" drawn
await 45 "a state file" saved
load "the state it just saved"
[ -z "${STATE_OUT:-}" ] || cp "$state" "$STATE_OUT"
earlier="none kept"
if [ -n "${PREVIOUS_STATE:-}" ]; then
  cat "$PREVIOUS_STATE" > "$state"
  load "the state saved under the lock before this one"
  earlier="loaded"
fi
kill -TERM "$launcher" 2>/dev/null || true
wait "$launcher" || true
grep -q 'honoured QUIT' "$work/launch.log" || fail "did not honour QUIT"
echo "$SYSTEM ($CORE): booted $name, first answer after $answered ms, frame spread $spread, state $(basename "$state") ($(wc -c < "$state") bytes), earlier lock's state: $earlier, quit cleanly"
