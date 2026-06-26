#!/usr/bin/env bash
set -u
export PATH=/nix/var/nix/profiles/default/bin:/run/current-system/sw/bin:/usr/bin:$PATH
export XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
DIR=/home/deck/.local/share/semu-bezel
rm -f /home/deck/bztest.done /home/deck/bztest.log; exec >/home/deck/bztest.log 2>&1
( while [ ! -f /home/deck/bztest.done ]; do /home/deck/.cache/semu-uinput-send l1 >/dev/null 2>&1 || true; sleep 35; done ) & KA=$!; trap 'kill $KA 2>/dev/null||true' EXIT
echo "== compile overlay =="
( cd "$DIR" && bash compile-on-deck.sh ) 2>&1 | tail -5
[ -x "$DIR/semu-bezel-overlay" ] || { echo "ABORT: compile failed"; touch /home/deck/bztest.done; exit 1; }
SEND=/home/deck/.cache/semu-uinput-send; GID=9855531406849998848
cap(){ busctl --user call org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.Screenshot Screenshot 'sa{sv}' '' 0 >/dev/null 2>&1; sleep 3; ls -t /home/deck/Pictures/Screenshot_*.png 2>/dev/null|head -1; }
ROM=$(find /run/media/deck/SD/Emulation -ipath "*/ROMs/ps2/*" \( -iname "*.iso" -o -iname "*.chd" -o -iname "*.bin" \) 2>/dev/null|grep -vi downloaded_media|head -1)
cat > /home/deck/semu-mb-launch.sh <<LAUNCH
#!/usr/bin/env bash
export ENABLE_GAMESCOPE_WSI=1 SEMU_BEZEL_LOG=/home/deck/bz-ovl.log
exec $DIR/semu-bezel-run.sh 4x3 flatpak run net.pcsx2.PCSX2 "$ROM" >>/home/deck/bz-run.log 2>&1
LAUNCH
chmod +x /home/deck/semu-mb-launch.sh; rm -f /home/deck/bz-ovl.log /home/deck/bz-run.log
flatpak kill net.pcsx2.PCSX2 2>/dev/null; pkill -f retroarch 2>/dev/null; pkill -x es-de 2>/dev/null
steam -shutdown >/dev/null 2>&1; for i in $(seq 1 45);do pgrep -x steam>/dev/null||break;sleep 1;done
for i in $(seq 1 90);do pgrep -x steam>/dev/null&&break;sleep 2;done; sleep 33
sp=$(pgrep -n steam); D=$(tr '\0' '\n'</proc/$sp/environ 2>/dev/null|sed -n 's/^DISPLAY=//p'|head -1); Bus=$(tr '\0' '\n'</proc/$sp/environ 2>/dev/null|sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p'|head -1)
env DISPLAY="$D" XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS="$Bus" steam "steam://rungameid/$GID" >/dev/null 2>&1 &
sleep 75
echo "ra/pcsx2 procs: pcsx2=$(pgrep -fc -i pcsx2) overlay=$(pgrep -fc semu-bezel-overlay)"
g=$(cap); cp "$g" /home/deck/verify-bezel.png 2>/dev/null; echo "CAP $(stat -c %s /home/deck/verify-bezel.png 2>/dev/null)"
echo "== overlay log =="; cat /home/deck/bz-ovl.log 2>/dev/null
flatpak kill net.pcsx2.PCSX2 2>/dev/null; pkill -f -i pcsx2 2>/dev/null; pkill -f semu-bezel-overlay 2>/dev/null
touch /home/deck/bztest.done
