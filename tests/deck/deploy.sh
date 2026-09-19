#!/bin/sh
# Deploy a release to the physical Steam Deck over SSH and drive the acceptance
# steps from the desktop. Needs DECK_HOST (ssh alias or user@host) and a built
# release at build/release.
#
#   tests/deck/deploy.sh install            # copy the tarball, run install.sh, print status
#   tests/deck/deploy.sh prepare            # run semu prepare on the Deck
#   tests/deck/deploy.sh launch EMULATOR SYSTEM ROM  # launch on the Deck, screenshot after 20 s
#   tests/deck/deploy.sh screenshot NAME    # gamescope screenshot type 3 into build/verification
set -eu
here="$(cd "$(dirname "$0")/../.." && pwd -P)"
deck="${DECK_HOST:?set DECK_HOST to the Deck's ssh target}"
release="$here/build/release"
verification="$here/build/verification/steam-deck"
run() { ssh -o BatchMode=yes "$deck" "$@"; }

screenshot() {  # gamescope screenshot type 3 is the composited frame; wait until the file stops growing
  name="$1"
  mkdir -p "$verification"
  run "rm -f /tmp/semu-shot.png; gamescopectl screenshot /tmp/semu-shot.png 3 >/dev/null 2>&1 || gamescope-screenshot /tmp/semu-shot.png 3; \
    last=0; for _ in 1 2 3 4 5 6 7 8 9 10; do sleep 0.5; size=\$(stat -c %s /tmp/semu-shot.png 2>/dev/null || echo 0); [ \"\$size\" -gt 0 ] && [ \"\$size\" = \"\$last\" ] && break; last=\$size; done"
  scp -q "$deck:/tmp/semu-shot.png" "$verification/$name.png"
  echo "$verification/$name.png"
}

case "${1:-}" in
  install)
    [ -f "$release/Semu-x86_64.tar.zst" ] || { echo "build the release first: nix build .#release --out-link build/release" >&2; exit 1; }
    run "mkdir -p ~/Downloads/semu"
    scp -q "$release/Semu-x86_64.tar.zst" "$release/Semu-x86_64.tar.zst.sha256" "$release/install.sh" "$deck:~/Downloads/semu/"
    run "sh ~/Downloads/semu/install.sh install ~/Downloads/semu/Semu-x86_64.tar.zst && sh ~/Downloads/semu/install.sh status"
    ;;
  prepare) run "~/Applications/Semu/bin/semu-deck-cli prepare --target steam-deck" ;;
  launch)
    emulator="${2:?emulator}"; system="${3:?system}"; rom="${4:?rom}"
    run "nohup ~/Applications/Semu/bin/semu-deck-cli launch '$emulator' --system '$system' --rom '$rom' >/tmp/semu-launch.log 2>&1 &"
    sleep 20
    screenshot "$system"
    run "tail -5 /tmp/semu-launch.log"
    ;;
  screenshot) screenshot "${2:-screen}" ;;
  *) echo "usage: deploy.sh install | prepare | launch EMULATOR SYSTEM ROM | screenshot NAME" >&2; exit 64 ;;
esac
