#!/usr/bin/env bash
# Deploy the standalone-emulator vk-layer bezels to the Deck + install the vkBasalt extension + verify.
# Runs from the Mac worktree; ssh's to the Deck. Idempotent.
set -u
W="/Users/alexanderschiffhauer/Library/CloudStorage/GoogleDrive-alexander.schiffhauer@gmail.com/My Drive/dev/semu/.claude/worktrees/infallible-roentgen-6f6029"
D="deck@steamdeck.local"; SSH="ssh -o BatchMode=yes -o ConnectTimeout=15 $D"
cd "$W" || exit 1
log(){ echo "[deploy-bezel $(date +%H:%M)] $*"; }
SRC=/home/deck/semu-codex
log "1/5 sync source + bezel assets"
scp -o BatchMode=yes src/semu/emulators/launcher.btrc "$D:$SRC/src/semu/emulators/launcher.btrc"
scp -o BatchMode=yes src/semu/tests/smoke.btrc "$D:$SRC/src/semu/tests/smoke.btrc"
scp -o BatchMode=yes generated/semu.c "$D:$SRC/generated/semu.c"
scp -o BatchMode=yes packaging/nix/semu.nix "$D:$SRC/packaging/nix/semu.nix"
$SSH "mkdir -p $SRC/packaging/standalone-bezel/reshade"
scp -o BatchMode=yes packaging/standalone-bezel/reshade/* "$D:$SRC/packaging/standalone-bezel/reshade/"
log "2/5 install vkBasalt extension for emulator runtimes"
$SSH 'bash -lc "
rts=\$(flatpak list --app --columns=application,runtime 2>/dev/null | grep -iE \"pcsx2|ryujinx|cemu|azahar|dolphin\" | awk \"{print \\\$NF}\" | awk -F/ \"{print \\\$NF}\" | sort -u)
for v in \$rts; do echo installing vkBasalt//\$v; flatpak install --user -y flathub org.freedesktop.Platform.VulkanLayer.vkBasalt//\$v 2>&1 | tail -1; done
flatpak list --columns=application | grep -i vkbasalt || echo NO_VKBASALT_INSTALLED
"'
log "3/5 rebuild + deploy bundle (--no-eval-cache)"
$SSH 'bash -lc "export XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus; systemctl --user reset-failed deploy3.service 2>/dev/null; systemd-run --user --unit=deploy3 --collect bash /home/deck/deploy3.sh"'
log "4/5 wait for rebuild"
for i in $(seq 1 60); do $SSH 'test -f /home/deck/deploy3.done && echo ok' 2>/dev/null | grep -q ok && { log "rebuild done"; break; }; sleep 30; done
B=$($SSH 'readlink -f /home/deck/result-codex')
log "bundle: $B ; bezel assets present: $($SSH "test -f $B/share/semu-bezel/reshade/Bezel.fx && echo yes || echo no")"
log "5/5 done — deployed. Run the bezel verify (launch a standalone game) next."

# 6/6 self-verify (only if the Deck is idle — won't interrupt a game)
log "6/6 verify bezel on-device"
scp -o BatchMode=yes "$W/packaging/deck-setup/verify-bezel-deck.sh" "$D:/home/deck/verify-bezel-deck.sh"
$SSH 'bash -lc "export XDG_RUNTIME_DIR=/run/user/1000 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus; systemctl --user reset-failed vbz.service 2>/dev/null; systemd-run --user --unit=vbz --collect bash /home/deck/verify-bezel-deck.sh"'
for i in $(seq 1 16); do $SSH 'test -f /home/deck/vbz.done && echo ok' 2>/dev/null | grep -q ok && break; sleep 12; done
$SSH 'cat /home/deck/vbz.log 2>/dev/null | tail -4'
scp -o BatchMode=yes "$D:/home/deck/verify-bezel.png" "$W/deck-shots/verify-bezel-ondeck.png" 2>/dev/null && log "pulled on-deck bezel capture -> deck-shots/verify-bezel-ondeck.png"
log "ALL DONE"
