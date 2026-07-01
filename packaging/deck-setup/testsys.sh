#!/usr/bin/env bash
# Per-system launch+capture+input harness. Drives the Semu launcher via the test Steam shortcut
# (so it presents in gamescope), captures the bezel/shader/sizing, injects a controller sequence,
# captures again (input check). Usage: testsys.sh <ra SYSTEM CORE | std EMU>
set -u
export XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
GID=9855531406849998848; SEND=/home/deck/.cache/semu-uinput-send
AI=/home/deck/Applications/Semu/Semu-x86_64.AppImage
CORES=/home/deck/.var/app/org.libretro.RetroArch/config/retroarch/cores
ROMS=/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
OUT=/home/deck/ts
rm -f "$OUT.png" "$OUT-before.png" "$OUT-after.png" "$OUT.log"; exec >"$OUT.log" 2>&1
cap(){ timeout 12 busctl --user call org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.Screenshot Screenshot 'sa{sv}' '' 0 >/dev/null 2>&1; sleep 2; ls -t /home/deck/Pictures/Screenshot_*.png 2>/dev/null|head -1; }
pick(){ ( set -f; find "$1" \( $2 \) -not -ipath '*/dlc/*' -not -ipath '*/update*' -printf '%s\t%p\n' 2>/dev/null ) | sort -rn | head -1 | cut -f2; }
MODE="$1"; PROC=""
if [ "$MODE" = ra ]; then
  SYS="$2"; CORE="$3"
  ROM=$(pick "$ROMS/$SYS" '-iname *.zip -o -iname *.sfc -o -iname *.smc -o -iname *.md -o -iname *.bin -o -iname *.gen -o -iname *.gb -o -iname *.gbc -o -iname *.gba -o -iname *.n64 -o -iname *.z64 -o -iname *.nes -o -iname *.nds -o -iname *.chd -o -iname *.cue')
  ARGS="launcher retroarch -f -L $CORES/${CORE}_libretro.so $ROM"; PROC=retroarch
else
  EMU="$2"
  case "$EMU" in
    pcsx2) PROC=pcsx2; ROM=$(pick "$ROMS/ps2" '-iname *.iso -o -iname *.chd' );;
    azahar) PROC=azahar; ROM=$(pick "$ROMS/n3ds" '-iname *.3ds -o -iname *.cci' );;
  esac
  ARGS="launcher $EMU $ROM"
fi
echo "MODE=$MODE ROM=$ROM"; echo "ARGS=$ARGS"
[ -z "$ROM" ] && { echo NO_ROM; exit 1; }
flatpak kill org.libretro.RetroArch net.pcsx2.PCSX2 org.ryujinx.Ryujinx info.cemu.Cemu org.azahar_emu.Azahar 2>/dev/null
pkill -f 'bin/semu launcher' 2>/dev/null; pkill -x es-de 2>/dev/null; pkill -x retroarch 2>/dev/null; sleep 4
printf '#!/usr/bin/env bash\nexec %s %s\n' "$AI" "$ARGS" > /home/deck/semu-mb-launch.sh; chmod +x /home/deck/semu-mb-launch.sh
sp=$(pgrep -n steam); D=$(tr '\0' '\n'</proc/$sp/environ|sed -n 's/^DISPLAY=//p'|head -1); Bus=$(tr '\0' '\n'</proc/$sp/environ|sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p'|head -1)
ok=0
for a in 1 2 3 4; do
  env DISPLAY="$D" XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS="$Bus" steam "steam://rungameid/$GID" >/dev/null 2>&1 &
  sleep 7; "$SEND" steam-a >/dev/null 2>&1
  for i in $(seq 1 10); do [ "$(pgrep -fc -i "$PROC")" -gt 0 ] && { ok=1;break; }; sleep 3; done
  [ "$ok" = 1 ] && { echo "up attempt $a"; break; }
done
sleep 35
g=$(cap); cp "$g" "$OUT-before.png" 2>/dev/null; echo "before CAP=$(stat -c%s "$OUT-before.png" 2>/dev/null)"
echo "--- inject input ---"; "$SEND" --delay-ms 350 --hold-ms 120 right right right down down a b a >/dev/null 2>&1; echo "send rc=$?"
sleep 4
g=$(cap); cp "$g" "$OUT-after.png" 2>/dev/null; echo "after CAP=$(stat -c%s "$OUT-after.png" 2>/dev/null)"
echo "procs=$(pgrep -fc -i "$PROC")"
echo "--- inner tail ---"; tail -6 /home/deck/.var/app/org.libretro.RetroArch/config/retroarch/retroarch.log 2>/dev/null
