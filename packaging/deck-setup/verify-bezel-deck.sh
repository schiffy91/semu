#!/usr/bin/env bash
set -u
export XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
SEND=/home/deck/.cache/semu-uinput-send; GID=9855531406849998848; SD=/run/media/deck/SD/Emulation
B=$(readlink -f /home/deck/result-codex)
rm -f /home/deck/vbz.done /home/deck/vbz.log; exec >/home/deck/vbz.log 2>&1
if pgrep -fi "pcsx2|ryujinx|cemu_rel|azahar|retroarch" >/dev/null 2>&1; then echo "SKIP: a game is running"; touch /home/deck/vbz.done; exit 0; fi
cap(){ timeout 12 busctl --user call org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.Screenshot Screenshot 'sa{sv}' '' 0 >/dev/null 2>&1; sleep 2; ls -t /home/deck/Pictures/Screenshot_*.png 2>/dev/null|head -1; }
ROM=$(find "$SD/ES-DE/ES-DE/ROMs/ps2" \( -iname "*.iso" -o -iname "*.chd" \) -printf "%s\t%p\n" 2>/dev/null|sort -n|head -1|cut -f2)
echo "ROM=$ROM ; vkbasalt installed: $(flatpak list --columns=application 2>/dev/null|grep -ci vkbasalt)"
cat > /home/deck/semu-mb-launch.sh <<L
#!/usr/bin/env bash
export ENABLE_GAMESCOPE_WSI=1
unset LD_PRELOAD LD_LIBRARY_PATH
export SEMU_PROJECT_DIR=/home/deck/.local/share/semu SEMU_ASSET_ROOT="$B"
[ -d "$B/config" ] && export SEMU_DEFINITION_ROOT="$B/config"
exec "$B/bin/semu" launcher pcsx2 "$ROM" >>/home/deck/vbz-inner.log 2>&1
L
chmod +x /home/deck/semu-mb-launch.sh; rm -f /home/deck/vbz-inner.log
flatpak kill net.pcsx2.PCSX2 2>/dev/null; pkill -9 -fi pcsx2 2>/dev/null; pkill -x es-de 2>/dev/null
steam -shutdown >/dev/null 2>&1; for i in $(seq 1 45); do pgrep -x steam>/dev/null||break; sleep 1; done
for i in $(seq 1 90); do pgrep -x steam>/dev/null&&break; sleep 2; done; sleep 33
for i in $(seq 1 30); do s=$(cap); sz=$(stat -c %s "$s" 2>/dev/null||echo 0); [ "${sz:-0}" -gt 90000 ]&&break; "$SEND" dpad-down dpad-up>/dev/null 2>&1; sleep 6; done
sp=$(pgrep -n steam); D=$(tr '\0' '\n'</proc/$sp/environ 2>/dev/null|sed -n 's/^DISPLAY=//p'|head -1); Bus=$(tr '\0' '\n'</proc/$sp/environ 2>/dev/null|sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p'|head -1)
env DISPLAY="$D" XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS="$Bus" steam "steam://rungameid/$GID">/dev/null 2>&1 &
sleep 90; g=$(cap); cp "$g" /home/deck/verify-bezel.png 2>/dev/null
echo "pcsx2 procs=$(pgrep -fc -i pcsx2) CAP=$(stat -c %s /home/deck/verify-bezel.png 2>/dev/null)"
flatpak kill net.pcsx2.PCSX2 2>/dev/null; pkill -9 -fi pcsx2 2>/dev/null
touch /home/deck/vbz.done
