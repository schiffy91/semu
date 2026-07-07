#!/usr/bin/env bash
# The closed visual loop, EVENT-DRIVEN: the tap is the tap-in point. Per
# combination: launch, wait for the tap's first composited frame (its log,
# not a guess), drop the 'semu-shot' trigger, and the tap itself writes the
# COMPOSITED framebuffer (game+bezel+shader+menu, pre-swap, pixel-exact) as
# semu-shot.bmp — no portal, no focus dependency, no fixed sleeps. Variants
# and shader cycle through the tap's own state files on the SAME running
# game; the emulator relaunches only per system.
set -u
PROJECT=/home/deck/.local/share/semu
APP=/home/deck/Applications/Semu/Semu-x86_64.AppImage
ROMS=/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
OUT="$PROJECT/src/generated/verification/loop"
STATE="$PROJECT/src/generated/runtime/flatpak-state/retroarch"
mkdir -p "$OUT" "$STATE"
"$APP" --appimage-mount > /tmp/loop-mount.txt 2>/dev/null & sleep 3
MOUNT=$(head -1 /tmp/loop-mount.txt)
export SEMU_PROJECT_DIR="$PROJECT" SEMU_ASSET_ROOT="$PROJECT/assets"   # project assets carry the CURRENT tap lib; the mount's is frozen at build time
PASS=0; FAIL=0
wait_for() { # wait_for <file> <max_seconds>
  local waited=0
  while [ ! -s "$1" ] && [ "$waited" -lt "$2" ]; do sleep 1; waited=$((waited+1)); done
  [ -s "$1" ]
}
for row in nes:mesen snes:snes9x gb:gambatte gba:mgba genesis:genesis_plus_gx n64:mupen64plus_next psx:mednafen_psx nds:desmume; do
  system="${row%%:*}"; core="${row##*:}"
  rom=$(ls "$ROMS/$system" 2>/dev/null | grep -viE "\.(txt|sav|srm)$" | head -1)
  [ -z "$rom" ] && { echo "SKIP $system (no rom)"; continue; }
  rm -f "$STATE/semutap.log" "$STATE/semu-shot" "$STATE/semu-shot.bmp" "$STATE"/semu-bezel "$STATE"/semu-shader "$STATE"/semu-align
  "$MOUNT/usr/bin/semu-retroarch" -f -L "$MOUNT/usr/lib/retroarch/cores/${core}_libretro.so" "$ROMS/$system/$rom" >/dev/null 2>&1 &
  if ! wait_for "$STATE/semutap.log" 30; then
    echo "FAIL $system (no tap frame in 30s)"; FAIL=$((FAIL+1))
    PIDS=$(ps aux | grep "[r]a-build/RetroArch" | awk '{print $2}'); for p in $PIDS; do kill -9 "$p" 2>/dev/null; done
    continue
  fi
  for variant in 0 1 2 9; do
    for shader in 1 0; do
      printf '%s' "$variant" > "$STATE/semu-bezel"
      printf '%s' "$shader"  > "$STATE/semu-shader"
      [ "${SEMU_LOOP_ALIGN:-0}" = "1" ] && printf 1 > "$STATE/semu-align"
      sleep 1                                   # let one frame absorb the state
      rm -f "$STATE/semu-shot.bmp"; touch "$STATE/semu-shot"
      if wait_for "$STATE/semu-shot.bmp" 5; then
        cp "$STATE/semu-shot.bmp" "$OUT/${system}-v${variant}-s${shader}.bmp"
        frame=$(grep "game=(" "$STATE/semutap.log" | tail -1)
        echo "PASS $system v$variant s$shader :: $frame"; PASS=$((PASS+1))
      else
        echo "FAIL $system v$variant s$shader (no composited shot)"; FAIL=$((FAIL+1))
      fi
    done
  done
  PIDS=$(ps aux | grep "[r]a-build/RetroArch" | awk '{print $2}')
  for p in $PIDS; do kill -9 "$p" 2>/dev/null; done
  sleep 1
done
echo "LOOP RESULT: pass=$PASS fail=$FAIL shots=$OUT"
