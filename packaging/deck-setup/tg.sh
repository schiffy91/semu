#!/usr/bin/env bash
# Fast bezel-iterate harness: writes a semu-deck preset (content on stdin), relaunches the system
# via the warm-shortcut hook, captures to /home/deck/tg.png. Usage: tg.sh SYS CORE  (preset on stdin)
set -u
export XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
SYS="$1"; CORE="$2"
CORES=/home/deck/.var/app/org.libretro.RetroArch/config/retroarch/cores
ROMS=/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs
SD=/home/deck/semu-run/usr/share/libretro/shaders/Mega_Bezel_Packs/semu-deck
SEND=/home/deck/.cache/semu-uinput-send
cap(){ timeout 12 busctl --user call org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.Screenshot Screenshot 'sa{sv}' '' 0 >/dev/null 2>&1; sleep 2; ls -t /home/deck/Pictures/Screenshot_*.png 2>/dev/null|head -1; }
NEW="$(cat)"; [ -n "$NEW" ] && { chmod -R u+w "$SD" 2>/dev/null; printf '%s\n' "$NEW" > "$SD/$SYS.slangp"; }
echo "preset: $(cat "$SD/$SYS.slangp" | tr '\n' '|')"
ROM=$( ( set -f; find "$ROMS/$SYS" \( -iname '*.zip' -o -iname '*.gba' -o -iname '*.gb' -o -iname '*.gbc' -o -iname '*.nes' -o -iname '*.sfc' -o -iname '*.smc' -o -iname '*.md' -o -iname '*.bin' -o -iname '*.gen' -o -iname '*.n64' -o -iname '*.z64' -o -iname '*.nds' -o -iname '*.chd' -o -iname '*.cue' \) -not -ipath '*/dlc/*' -printf '%s\t%p\n' 2>/dev/null ) | sort -rn | head -1 | cut -f2 )
echo "ROM=$ROM"; [ -z "$ROM" ] && { echo NO_ROM; exit 1; }
flatpak kill org.libretro.RetroArch 2>/dev/null; pkill -f 'bin/semu launcher' 2>/dev/null; pkill -x retroarch 2>/dev/null; sleep 3
cat > /tmp/semu-test-cmd <<L
#!/usr/bin/env bash
exec /home/deck/semu-run/usr/bin/semu launcher retroarch -f -L "$CORES/${CORE}_libretro.so" "$ROM"
L
sp=$(pgrep -n steam); D=$(tr '\0' '\n'</proc/$sp/environ|sed -n 's/^DISPLAY=//p'|head -1); Bus=$(tr '\0' '\n'</proc/$sp/environ|sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p'|head -1)
ok=0
for a in 1 2 3 4 5; do
  env DISPLAY="$D" XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS="$Bus" steam "steam://rungameid/13124415270485491712" >/dev/null 2>&1 &
  for i in $(seq 1 8); do [ "$(pgrep -fc -i retroarch)" -gt 0 ] && { ok=1; break; }; sleep 3; done
  [ "$ok" = 1 ] && { echo "launched attempt $a"; break; }
done
sleep 24
g=$(cap); cp "$g" /home/deck/tg.png 2>/dev/null; echo "CAP=$(stat -c%s /home/deck/tg.png 2>/dev/null) ra=$(pgrep -fc -i retroarch)"
