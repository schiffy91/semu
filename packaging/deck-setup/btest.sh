#!/usr/bin/env bash
# Standalone-emulator bezel test: launch a known-good game via the real Semu launcher
# (through the Steam shortcut so it lands in the gamescope session), then prove vkBasalt
# is active programmatically (the launcher's ENABLE_VKBASALT env + generated conf +
# libvkbasalt actually mapped into the render process) and visually (screen capture).
# Usage: btest.sh <pcsx2|ryujinx|cemu|azahar>
# NOTE: never pkill/pgrep -f on $EMU -- the script's own argv contains it (self-kill).
set -u
export XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
GID=9855531406849998848
SD=/run/media/deck/SD/Emulation
ROMS="$SD/ES-DE/ES-DE/ROMs"
B=$(readlink -f /home/deck/result-codex)
EMU="${1:-pcsx2}"
WAIT="${2:-70}"   # seconds to let the game render before capture (heavier emus need more)
OUT=/home/deck/btest
rm -f "$OUT.done" "$OUT.log" "$OUT.png"; exec >"$OUT.log" 2>&1
cap(){ timeout 12 busctl --user call org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop org.freedesktop.portal.Screenshot Screenshot 'sa{sv}' '' 0 >/dev/null 2>&1; sleep 2; ls -t /home/deck/Pictures/Screenshot_*.png 2>/dev/null|head -1; }
# find render procs the launcher tagged with ENABLE_VKBASALT=1 (NOT this script)
vkpids(){ for e in /proc/[0-9]*/environ; do grep -qa 'ENABLE_VKBASALT=1' "$e" 2>/dev/null && { p=${e#/proc/}; echo "${p%/environ}"; }; done; }
# base-game picker: match name, right extensions, exclude dlc/update/demo, take the largest
pick(){ ( set -f; find "$1" \( $2 \) -iname "$3" -not -ipath '*/dlc/*' -not -ipath '*/update*' -not -ipath '*/demo*' -printf '%s\t%p\n' 2>/dev/null ) | sort -rn | head -1 | cut -f2; }
case "$EMU" in
  pcsx2)   FID=net.pcsx2.PCSX2;       ROM=$(pick "$ROMS/ps2"    '-iname *.iso -o -iname *.chd' 'Marvel vs*');;
  ryujinx) FID=org.ryujinx.Ryujinx;   ROM=$(pick "$ROMS/switch" '-iname *.xci -o -iname *.nsp' '*Animal Crossing*');;
  cemu)    FID=info.cemu.Cemu;        ROM=$(pick "$ROMS/wiiu"   '-iname *.wua -o -iname *.wud -o -iname *.rpx -o -iname *.iso' '*New Super Mario*');;
  azahar)  FID=org.azahar_emu.Azahar; ROM=$(pick "$ROMS/n3ds"   '-iname *.3ds -o -iname *.cci' 'Retro City Rampage*');;
  *) echo "unknown emu $EMU"; touch "$OUT.done"; exit 1;;
esac
echo "EMU=$EMU bundle=$B"
echo "ROM=$ROM"
[ -z "$ROM" ] && { echo "NO ROM FOUND"; touch "$OUT.done"; exit 1; }
cat > /home/deck/semu-mb-launch.sh <<L
#!/usr/bin/env bash
export ENABLE_GAMESCOPE_WSI=1
unset LD_PRELOAD LD_LIBRARY_PATH
export SEMU_PROJECT_DIR=/home/deck/.local/share/semu SEMU_ASSET_ROOT="$B"
exec "$B/bin/semu" launcher $EMU "$ROM" >>/home/deck/btest-inner.log 2>&1
L
chmod +x /home/deck/semu-mb-launch.sh; rm -f /home/deck/btest-inner.log
# kill ALL standalone emulators so a lingering one isn't mis-detected as this run
for f in net.pcsx2.PCSX2 org.ryujinx.Ryujinx info.cemu.Cemu org.azahar_emu.Azahar; do flatpak kill "$f" 2>/dev/null; done; sleep 4
sp=$(pgrep -n steam); D=$(tr '\0' '\n'</proc/$sp/environ|sed -n 's/^DISPLAY=//p'|head -1); Bus=$(tr '\0' '\n'</proc/$sp/environ|sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p'|head -1)
echo "DISPLAY=$D ; launching via steam shortcut"
env DISPLAY="$D" XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS="$Bus" steam "steam://rungameid/$GID" >/dev/null 2>&1 &
# wait until the launcher has spawned a vkBasalt-tagged emulator process
pid=""
for i in $(seq 1 50); do pid=$(vkpids | head -1); [ -n "$pid" ] && break; sleep 3; done
echo "first vkBasalt-tagged pid=$pid (after ~$((i*3))s)"
if [ -n "$pid" ]; then
  echo "--- VKBASALT env ---"; tr '\0' '\n' </proc/$pid/environ 2>/dev/null | grep -iE 'VKBASALT|GAMESCOPE_WSI'
  cfg=$(tr '\0' '\n' </proc/$pid/environ 2>/dev/null | sed -n 's/^VKBASALT_CONFIG_FILE=//p' | head -1)
  echo "--- vkBasalt.conf [$cfg] ---"; cat "$cfg" 2>/dev/null
fi
# capture progressively so a late-rendering game still yields a gameplay frame
half=$(( WAIT/2 )); [ $half -lt 35 ] && half=35
sleep "$half"; g=$(cap); cp "$g" "$OUT-mid.png" 2>/dev/null; echo "mid CAP=$(stat -c%s "$OUT-mid.png" 2>/dev/null)"
sleep "$half"
echo "--- libvkbasalt mapped into a render proc? ---"
ACTIVE=no
for p in $(vkpids); do
  cl=$(tr '\0' ' ' </proc/$p/cmdline 2>/dev/null | cut -c1-50)
  if grep -qi vkbasalt /proc/$p/maps 2>/dev/null; then echo "  pid=$p VKBASALT-LOADED  cmd=$cl"; ACTIVE=yes; else echo "  pid=$p (env only)     cmd=$cl"; fi
done
echo "VKBASALT_ACTIVE=$ACTIVE"
g=$(cap); cp "$g" "$OUT.png" 2>/dev/null
echo "running-emu-procs=$(vkpids | wc -l) CAP=$(stat -c%s "$OUT.png" 2>/dev/null)"
echo "inner-tail:"; tail -8 /home/deck/btest-inner.log 2>/dev/null
touch "$OUT.done"
