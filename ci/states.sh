#!/usr/bin/env bash
# Keeps the state each real core saved under this lock (tests/integration/states) as the one the next lock
# has to load. The check `real-cores` loads the kept state after its own, and fails where a core no longer
# takes it: that failure is the news that an update breaks the states people already have. Run this once
# that is accepted, or after adding a core; then commit the states.
#   ci/states.sh                 every core
#   ci/states.sh psx-mednafen_psx n64-mupen64plus_next    the ones named
set -eu
cd "$(dirname "$0")/.."
kept=tests/integration/states
system=$(nix eval --raw --impure --expr builtins.currentSystem)
[ "$#" -gt 0 ] || set -- $(nix eval --raw ".#checks.$system.real-cores.entries" --apply 'entries: builtins.concatStringsSep " " (builtins.attrNames entries)')
mkdir -p "$kept"
for name in "$@"; do rm -f "$kept/$name.state"; done  # a core that refuses its kept state cannot save a new one beside it
for name in "$@"; do
  run=$(nix build --no-link --print-out-paths ".#checks.$system.real-cores.entries.$name")
  install -m644 "$run/state" "$kept/$name.state"
done
git add --intent-to-add "$kept"
git status --short "$kept"
