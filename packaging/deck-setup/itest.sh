#!/usr/bin/env bash
# Input test: launch a game via the real Semu launcher, capture a "before" frame, inject a
# controller sequence via the uinput helper, capture "after". Proves input both programmatically
# (which /dev/input/event* the emulator has open) and visually (game state changes on input).
# Usage: itest.sh <pcsx2|ryujinx|cemu|azahar|retroarch> [wait_s] [actions...]
set -u
export XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
GID=9855531406849998848
SD=/run/media/deck/SD/Emulation
ROMS="$SD/ES-DE/ES-DE/ROMs"
B=$(readlink -f /home/deck/result-codex)
SEND=/home/deck/.cache/semu-uinput-send
EMU="${1:-pcsx2}"; WAIT="${2:-75}"; shift || true; shift || true
ACTIONS=("$@"); [ ${#ACTIONS[@]} -eq 0 ] && ACTIONS=(start start a a right right a)
OUT=/home/deck/itest
rm -f "$OUT.done" "$OUT.log" "$OUT-before.png" "$OUT-after.png"; exec >"$OUT.log" 2>&1
cap(){ timeout 12 busctl --user call org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.Screenshot Screenshot 'sa{sv}' '' 0 >/dev/null 2>&1; sleep 2; ls -t /home/deck/Pictures/Screenshot_*.png 2>/dev/null|head -1; }
vkpids(){ for e in /proc/[0-9]*/environ; do grep -qa 'ENABLE_VKBASALT=1' "$e" 2>/dev/null && { p=${e#/proc/}; echo "${p%/environ}"; }; done; }
# generic emulator-process finder: vkBasalt-tagged (standalone) OR matching the flatpak id (RA)
emupids(){ { vkpids; pgrep -f "$FID" 2>/dev/null; pgrep -x retroarch 2>/dev/null; } | sort -un; }
pick(){ ( set -f; find "$1" \( $2 \) -iname "$3" -not -ipath '*/dlc/*' -not -ipath '*/update*' -not -ipath '*/demo*' -printf '%s\t%p\n' 2>/dev/null ) | sort -rn | head -1 | cut -f2; }
case "$EMU" in
  pcsx2)   FID=net.pcsx2.PCSX2;       ROM=$(pick "$ROMS/ps2"    '-iname *.iso -o -iname *.chd' 'Marvel vs*');;
  ryujinx) FID=org.ryujinx.Ryujinx;   ROM=$(pick "$ROMS/switch" '-iname *.xci -o -iname *.nsp' '*Animal Crossing*');;
  cemu)    FID=info.cemu.Cemu;        ROM=$(pick "$ROMS/wiiu"   '-iname *.wua -o -iname *.wud -o -iname *.rpx' '*New Super Mario*');;
  azahar)  FID=org.azahar_emu.Azahar; ROM=$(pick "$ROMS/n3ds"   '-iname *.3ds -o -iname *.cci' 'Retro City Rampage*');;
  retroarch) FID=org.libretro.RetroArch; ROM=$(pick "$ROMS/snes" '-iname *.sfc -o -iname *.smc -o -iname *.zip' 'Super Mario World (USA)*');;
  *) echo "unknown emu $EMU"; touch "$OUT.done"; exit 1;;
esac
# RetroArch needs an explicit core (es-de supplies "-f -L <core> <rom>"); a bare rom won't run.
EXTRA=""; [ "$EMU" = retroarch ] && EXTRA="-f -L /home/deck/.var/app/org.libretro.RetroArch/config/retroarch/cores/snes9x_libretro.so"
echo "EMU=$EMU ROM=$ROM actions=${ACTIONS[*]}"
[ -z "$ROM" ] && { echo "NO ROM"; touch "$OUT.done"; exit 1; }
# pause the net-zero keepalive so it doesn't pollute the before/after frames
systemctl --user stop semu-keepalive 2>/dev/null
# expose a PERSISTENT virtual pad (fifo-driven) so it's present at the emulator's startup
# controller scan and gets bound; transient per-call devices can be missed by RA's hotplug.
rm -f /tmp/semupad; setsid "$SEND" --fifo /tmp/semupad >/dev/null 2>&1 & sleep 1
cat > /home/deck/semu-mb-launch.sh <<L
#!/usr/bin/env bash
export ENABLE_GAMESCOPE_WSI=1
unset LD_PRELOAD LD_LIBRARY_PATH
export SEMU_PROJECT_DIR=/home/deck/.local/share/semu SEMU_ASSET_ROOT="$B"
exec "$B/bin/semu" launcher $EMU $EXTRA "$ROM" >>/home/deck/itest-inner.log 2>&1
L
chmod +x /home/deck/semu-mb-launch.sh; rm -f /home/deck/itest-inner.log
# Proven recipe (from btest, no Steam restart): flatpak kill the prior app so Steam registers
# the shortcut as stopped, then rungameid launches fresh. (A Steam restart makes rungameid only
# navigate to the game page instead of launching.)
for f in net.pcsx2.PCSX2 org.ryujinx.Ryujinx info.cemu.Cemu org.azahar_emu.Azahar org.libretro.RetroArch; do flatpak kill "$f" 2>/dev/null; done
sleep 6
sp=$(pgrep -n steam); D=$(tr '\0' '\n'</proc/$sp/environ|sed -n 's/^DISPLAY=//p'|head -1); Bus=$(tr '\0' '\n'</proc/$sp/environ|sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p'|head -1)
env DISPLAY="$D" XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS="$Bus" steam "steam://rungameid/$GID" >/dev/null 2>&1 &
pid=""; for i in $(seq 1 50); do pid=$(emupids|head -1); [ -n "$pid" ] && break; sleep 3; done
echo "emu pid=$pid"
sleep "$WAIT"
echo "--- /dev/input/by-id ---"; ls -1 /dev/input/by-id/ 2>/dev/null
echo "--- evdev event devices open by emulator proc(s) ---"
op=0
for p in $(emupids); do for fd in /proc/$p/fd/*; do t=$(readlink "$fd" 2>/dev/null); case "$t" in /dev/input/event*) echo "  pid=$p -> $t"; op=$((op+1));; esac; done; done
echo "evdev-handles-open=$op"
g=$(cap); cp "$g" "$OUT-before.png"; echo "before CAP=$(stat -c%s "$OUT-before.png" 2>/dev/null)"
echo "--- injecting: ${ACTIONS[*]} (persistent fifo pad + transient) ---"
for a in "${ACTIONS[@]}"; do printf '%s\n' "$a" > /tmp/semupad 2>/dev/null; sleep 0.5; done
sleep 1
"$SEND" --delay-ms 450 --hold-ms 140 "${ACTIONS[@]}" 2>&1; echo "send1 rc=$?"
sleep 2
"$SEND" --delay-ms 450 --hold-ms 140 start steam-a steam-a 2>&1; echo "send2 rc=$?"
sleep 4
g=$(cap); cp "$g" "$OUT-after.png"; echo "after CAP=$(stat -c%s "$OUT-after.png" 2>/dev/null)"
pkill -f 'semu-uinput-send --fifo' 2>/dev/null; rm -f /tmp/semupad
systemctl --user start semu-keepalive 2>/dev/null
touch "$OUT.done"
