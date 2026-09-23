#!/bin/sh
# Every RetroArch system through `semu launch`, with the synthetic core standing in for each
# system's own core file: the launch must bring up RetroArch's command port, and stopping the
# launcher must end RetroArch through its own QUIT with nothing left of the session.
# Inputs: SEMU_CLI (semu with its config), RETROARCH, CORE (synthetic), RENDERER (renderer lib dir).
set -eu
work="${TMPDIR:-/tmp}/semu-launch-systems"
config="$(dirname "$(readlink -f "$SEMU_CLI")")/../share/semu/config"
[ -d "$config/systems" ] || config="$SEMU_SOURCE_ROOT"
command_port() { printf '%s' "$1" | socat -t 1 - UDP:127.0.0.1:55355 2>/dev/null | tr -d '\n'; }
rm -rf "$work"
mkdir -p "$work/home" "$work/assets/bin" "$work/assets/lib/retroarch/cores"
export HOME="$work/home" LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe
unset WAYLAND_DISPLAY
ln -s "$RETROARCH" "$work/assets/bin/retroarch"
ln -s "$RENDERER/libsemurenderer.so" "$work/assets/lib/libsemurenderer.so"
settings="{\"paths\":{\"roms\":\"$work/roms\",\"state_root\":\"$work/state\",\"content_root\":\"$work/content\"}}"
failures=0
for definition in "$config"/systems/*/system.json; do
  system="$(basename "$(dirname "$definition")")"
  core="$(jq -r '[.emulators[] | select(.emulator == "retroarch" and ((.platforms // ["linux"]) | index("linux")))][0].core // empty' "$definition")"
  [ -n "$core" ] || continue
  ln -sf "$CORE" "$work/assets/lib/retroarch/cores/${core}_libretro.so"
  romDirectory="$work/roms/$(jq -r '.rom.dir' "$definition")"
  mkdir -p "$romDirectory"
  printf 'semu synthetic content\n' > "$romDirectory/pattern.semu"
  "$SEMU_CLI" launch retroarch --system "$system" --rom pattern.semu --asset-root "$work/assets" --settings-json "$settings" > "$work/$system.log" 2>&1 &
  launcher=$!
  version=""
  for _ in $(seq 1 80); do
    version="$(command_port VERSION || true)"
    [ -n "$version" ] && break
    kill -0 "$launcher" 2>/dev/null || break
    sleep 0.5
  done
  retroarch="$(pgrep -f "$work/state" | head -1 || true)"
  kill -TERM "$launcher" 2>/dev/null || true
  status=0
  wait "$launcher" || status=$?
  sleep 0.5
  leftover="$(pgrep -f "$work/state" || true)"
  if [ -z "$version" ]; then echo "$system ($core): RetroArch never answered VERSION"; tail -5 "$work/$system.log"; failures=$((failures + 1))
  elif [ -n "$leftover" ]; then echo "$system ($core): RetroArch outlived its launcher"; kill -KILL $leftover 2>/dev/null || true; failures=$((failures + 1))
  elif ! grep -q 'honoured QUIT' "$work/$system.log"; then echo "$system ($core): RetroArch did not honour QUIT (launcher status $status, pid ${retroarch:-none})"; failures=$((failures + 1))
  else echo "$system ($core): launched, answered $version, quit cleanly"; fi
done
[ "$failures" -eq 0 ] && echo "launch-systems: pass" || { echo "launch-systems: $failures failed"; exit 1; }
