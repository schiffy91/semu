#!/bin/sh
# Builds the Steam Deck release (x86_64-linux) on a Mac, inside podman's Linux VM under Rosetta,
# and links it at build/release, where `tests/deck/deploy.sh install` picks it up. Safe to rerun
# after the Mac slept or rebooted: the VM's Nix store volume keeps every finished package, so only
# unfinished ones build again. The source is the committed HEAD (uncommitted edits are not built).
# Work and output live in ~/.cache/semu-release, outside the synced checkout.
#
#   tests/deck/build-release.sh            # needs the network for anything not yet downloaded
#   tests/deck/build-release.sh --offline  # no network: uses only what the VM already has (each
#                                          # online run first copies every flake input into the store)
set -eu
here="$(cd "$(dirname "$0")/../.." && pwd -P)"
cache="$HOME/.cache/semu-release"
offline=""
archive="nix flake archive git+file:///src > /out/archive.log 2>&1 &&"
[ "${1:-}" = "--offline" ] && { offline="--offline"; archive=""; }

[ "$(podman machine inspect --format '{{.State}}')" = running ] || podman machine start
# Rosetta runs the x86_64 toolchain; the VM's qemu fallback crashes Nix
podman machine ssh 'test -e /proc/sys/fs/binfmt_misc/rosetta || { sudo touch /etc/containers/enable-rosetta && sudo systemctl start rosetta-activation.service; }'
for volume in semu-nix-x86 semu-nix-cache; do podman volume exists "$volume" || podman volume create "$volume" >/dev/null; done  # the store, and the fetched flake inputs --offline needs
if [ "$(podman inspect --format '{{.State.Running}}' semu-release-build 2>/dev/null)" = true ]; then
  echo "a release build is already running; waiting for it (log: $cache/out/build.log)"
  podman wait semu-release-build >/dev/null
fi
podman rm -f semu-release-build >/dev/null 2>&1 || true

rm -rf "$cache/source"
mkdir -p "$cache/out"
git clone -q "$here" "$cache/source"
echo "building $(git -C "$cache/source" log --oneline -1) (log: $cache/out/build.log)"
podman run --rm --name semu-release-build --platform linux/amd64 --privileged \
  -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix -v "$cache/source:/src:ro" -v "$cache/out:/out" \
  -e NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0" \
  docker.io/nixos/nix:latest sh -c "git config --global --add safe.directory '*' \
    && $archive nix build -L $offline --out-link /tmp/release 'git+file:///src#packages.x86_64-linux.release' > /out/build.log 2>&1 \
    && rm -f /out/Semu-x86_64.tar.zst* /out/install.sh && cp -L /tmp/release/* /out/" \
  || { tail -30 "$cache/out/build.log"; echo "build failed; log: $cache/out/build.log" >&2; exit 1; }
mkdir -p "$here/build"
ln -sfn "$cache/out" "$here/build/release"
ls -lh "$cache/out"/Semu-x86_64.tar.zst*
echo "next: DECK_HOST=deck@steamdeck.local tests/deck/deploy.sh install"
