#!/bin/bash
# ES-DE on the Steam Deck, off-screen: the installed release started the way Steam starts it
# (with Steam's overlay preloaded) inside a private headless gamescope at 1280x800 with the sound
# cut, driven by keys sent only to that gamescope's X display. Captures the start (where a broken
# configuration shows its popup), a system's game list, a game launched from it, ES-DE after the
# game quits the way Semu quits it, and ES-DE moving after that. PIDs only; nothing is removed.
#
#   esde-tour.sh OUT [SYSTEM-STEPS]   # SYSTEM-STEPS: Right presses from the first system (default 0)
set -u
out="$1"; steps="${2:-0}"
steam="$HOME/.local/share/Steam"; esde_log=/run/media/deck/SD/Emulation/ES-DE/ES-DE/logs/es_log.txt
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
mkdir -p "$out"; : > "$out/result"
note() { echo "$*" >> "$out/result"; }
descendants() { local child; for child in $(pgrep -P "$1"); do echo "$child"; descendants "$child"; done; }

cat > "$out/inner.sh" <<INNER
#!/bin/sh
export LD_PRELOAD="$steam/steamrt32/gameoverlayrenderer.so:$steam/steamrt64/gameoverlayrenderer.so"
exec "$HOME/Applications/Semu/bin/semu-deck"
INNER
chmod +x "$out/inner.sh"
start=$(date +%s)
PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none SEMU_MATRIX_ROM=esde-tour \
  gamescope --backend headless -W 1280 -H 800 -w 1280 -h 800 -- "$out/inner.sh" > "$out/run.log" 2>&1 &
headless=$!
esde=""; for _ in $(seq 1 90); do
  for pid in $(descendants "$headless"); do [ "$(cat "/proc/$pid/comm" 2>/dev/null)" = es-de ] && esde=$pid; done
  [ -n "$esde" ] && break; sleep 1
done
[ -n "$esde" ] || { note "es-de: never started"; kill -TERM "$headless"; exit 1; }
for _ in $(seq 1 120); do
  grep -q "Application startup time" "$esde_log" 2>/dev/null && [ "$(stat -c %Y "$esde_log")" -ge "$start" ] && break; sleep 1
done
note "es-de: pid $esde; $(grep 'Application startup time' "$esde_log" | tail -1 | cut -c17-) ($(( $(date +%s) - start )) s after launch)"
sleep 4
display="$(tr '\0' '\n' < "/proc/$esde/environ" | sed -n 's/^DISPLAY=//p')"
wayland="$(tr '\0' '\n' < "/proc/$esde/environ" | sed -n 's/^GAMESCOPE_WAYLAND_DISPLAY=//p')"
key() { DISPLAY="$display" xdotool key --delay 200 "$@"; }
shoot() {
  GAMESCOPE_WAYLAND_DISPLAY="$wayland" timeout 20 gamescopectl screenshot "$out/$1.png" > /dev/null 2>&1
  for _ in $(seq 1 10); do [ -s "$out/$1.png" ] && break; sleep 1; done
  note "shot $1: $([ -s "$out/$1.png" ] && echo yes || echo no)"
}
shoot 1-start
for _ in $(seq 1 "$steps"); do key Right; done; sleep 3
shoot 2-system
key Return; sleep 6
shoot 3-gamelist
lines=$(wc -l < "$esde_log")
key Return  # launch the selected game
game=""; for _ in $(seq 1 60); do
  for pid in $(descendants "$headless"); do [ "$(cat "/proc/$pid/comm" 2>/dev/null)" = semu-btrc ] && game=$pid; done
  [ -n "$game" ] && break; sleep 1
done
tail -n +$((lines + 1)) "$esde_log" | grep -E "Launching|Expanded|Warn|Error" | cut -c17-240 | head -4 | sed 's/^/es_log: /' >> "$out/result"
if [ -n "$game" ]; then
  note "game: semu-btrc pid $game"
  sleep 40; shoot 4-game
  kill -TERM "$game"; for _ in $(seq 1 40); do kill -0 "$game" 2>/dev/null || break; sleep 0.5; done
  kill -0 "$game" 2>/dev/null && note "quit: semu-btrc still running 20 s after SIGTERM" || note "quit: clean"
else
  note "game: none started"
fi
sleep 8; shoot 5-after-game
key Down Down Down; sleep 3; shoot 6-moved
note "es-de main thread after the game: $(awk '{print $3}' "/proc/$esde/task/$esde/stat" 2>/dev/null), $(ps -o pcpu= -p "$esde" 2>/dev/null | tr -d ' ')% cpu"
grep -E "Error|Warn" "$esde_log" | grep -v -i "font\|theme" | tail -6 | cut -c17-240 | sed 's/^/es_log: /' >> "$out/result"

for pid in $(descendants "$headless"); do [ "$(cat "/proc/$pid/comm" 2>/dev/null)" = es-de ] && kill -TERM "$pid"; done
for _ in $(seq 1 30); do kill -0 "$headless" 2>/dev/null || break; sleep 0.5; done
if kill -0 "$headless" 2>/dev/null; then
  for pid in $(descendants "$headless"); do kill -TERM "$pid" 2>/dev/null; done
  kill -TERM "$headless" 2>/dev/null; sleep 3
  for pid in $(descendants "$headless") "$headless"; do kill -0 "$pid" 2>/dev/null && kill -KILL "$pid" 2>/dev/null; done
  note "cleanup: the private gamescope had to be stopped"
fi
date > "$out/done"
