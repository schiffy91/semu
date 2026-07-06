#!/usr/bin/env bash
# Runs ON the Deck (piped over ssh by the visual-loop make target).
set -u
PROJECT=/home/deck/.local/share/semu
APP=/home/deck/Applications/Semu/Semu-x86_64.AppImage
ROMS=/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
OUT="$PROJECT/src/generated/verification/loop"
mkdir -p "$OUT"
"$APP" --appimage-mount > /tmp/loop-mount.txt 2>/dev/null & sleep 3
MOUNT=$(head -1 /tmp/loop-mount.txt)
export SEMU_PROJECT_DIR="$PROJECT" SEMU_ASSET_ROOT="$MOUNT"
PASS=0; FAIL=0
# system:core rows for the RetroArch-driven set (contract cores, SD roms)
for row in nes:mesen snes:snes9x gb:gambatte gba:mgba genesis:genesis_plus_gx n64:mupen64plus_next psx:mednafen_psx nds:desmume; do
  system="${row%%:*}"; core="${row##*:}"
  rom=$(ls "$ROMS/$system" 2>/dev/null | grep -viE "\.(txt|sav|srm)$" | head -1)
  [ -z "$rom" ] && { echo "SKIP $system (no rom)"; continue; }
  state="$PROJECT/src/generated/runtime/flatpak-state/retroarch"
  for variant in 0 1 2 9; do   # 9 = past count -> bezel OFF
    for shader in 1 0; do
      rm -f "$state/semutap.log"
      mkdir -p "$state"
      printf '%s' "$variant" > "$state/semu-bezel"
      printf '%s' "$shader"  > "$state/semu-shader"
      [ "${SEMU_LOOP_ALIGN:-0}" = "1" ] && printf 1 > "$state/semu-align" || rm -f "$state/semu-align"
      "$MOUNT/usr/bin/semu-retroarch" -f -L "$MOUNT/usr/lib/retroarch/cores/${core}_libretro.so" "$ROMS/$system/$rom" >/dev/null 2>&1 &
      sleep 22
      frame=$(grep "game=(" "$state/semutap.log" 2>/dev/null | tail -1)
      shot="$OUT/${system}-v${variant}-s${shader}.png"
      timeout 12 busctl --user call org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.Screenshot Screenshot "sa{sv}" "" 1 interactive b false >/dev/null 2>&1
      sleep 2
      latest=$(ls -t /home/deck/Pictures/Screenshot_*.png 2>/dev/null | head -1)
      [ -n "$latest" ] && cp "$latest" "$shot"
      /home/deck/.cache/semu-uinput-send select-start 2>/dev/null; sleep 4
      pids=$(ps aux | grep "[r]a-build/RetroArch" | awk '{print $2}')
      for p in $pids; do kill -9 "$p" 2>/dev/null; done
      if [ -n "$frame" ]; then echo "PASS $system v$variant s$shader :: $frame"; PASS=$((PASS+1));
      else echo "FAIL $system v$variant s$shader :: no tap frame"; FAIL=$((FAIL+1)); fi
    done
  done
done
echo "LOOP RESULT: pass=$PASS fail=$FAIL shots=$OUT"
