#!/bin/sh
# Builds the Steam Deck release (x86_64-linux) on a Mac, inside podman's Linux VM under Rosetta,
# and links it at build/release, where `tests/deck/deploy.sh install` picks it up. Safe to rerun
# after the Mac slept or rebooted: the VM's Nix store volume keeps every finished package, so only
# unfinished ones build again. The source is the committed HEAD (uncommitted edits are not built).
# Work and output live in ~/.cache/semu-release, outside the synced checkout.
#
#   tests/deck/build-release.sh               # needs the network for anything not yet downloaded
#   tests/deck/build-release.sh --offline     # no network: uses only what the VM already has (each
#                                             # online run first copies every flake input into the store)
#   tests/deck/build-release.sh --delta HAVE  # no tarball: the release tree plus only the store paths
#                                             # not named in HAVE (`deploy.sh store-list` on the Deck),
#                                             # for `deploy.sh install-delta`; combine with --offline
set -eu
here="$(cd "$(dirname "$0")/../.." && pwd -P)"
cache="$HOME/.cache/semu-release"
offline=""
archive="nix flake archive git+file:///src > /out/archive.log 2>&1 &&"
have=""
while [ $# -gt 0 ]; do
  case "$1" in
    --offline) offline="--offline"; archive="" ;;
    --delta) have="$(cd "$(dirname "${2:?--delta needs the Deck store-list}")" && pwd -P)/$(basename "$2")"; shift ;;
    *) echo "usage: build-release.sh [--offline] [--delta HAVE]" >&2; exit 64 ;;
  esac
  shift
done

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
if [ -n "$have" ]; then
  # The tree (launchers, installer, version, store-paths) and every store path the Deck lacks,
  # packed straight out of the VM's store; the Deck hard-links the rest from its current release.
  build="nix build -L $offline --out-link /tmp/tree 'git+file:///src#packages.x86_64-linux.release-tree' > /out/build.log 2>&1 \
    && zstd=\$(nix build $offline --no-link --print-out-paths --inputs-from /src nixpkgs#zstd.bin)/bin/zstd \
    && sort /have > /tmp/have && comm -23 /tmp/tree/store-paths /tmp/have > /out/delta-paths \
    && rm -f /out/Semu-x86_64.delta.tar.zst /out/Semu-x86_64.delta.tar.zst.sha256 /out/install.sh \
    && while read -r name; do printf 'nix/store/%s\\n' \"\$name\"; done < /out/delta-paths > /tmp/delta-list \
    && (set -o pipefail; tar -cf - -C /tmp/tree . -C / -T /tmp/delta-list | \$zstd -q -T0 -3 -o /out/Semu-x86_64.delta.tar.zst) \
    && (cd /out && sha256sum Semu-x86_64.delta.tar.zst > Semu-x86_64.delta.tar.zst.sha256) \
    && cp /tmp/tree/install.sh /out/install.sh"
  mounts="-v $have:/have:ro"
else
  build="nix build -L $offline --out-link /tmp/release 'git+file:///src#packages.x86_64-linux.release' > /out/build.log 2>&1 \
    && rm -f /out/Semu-x86_64.tar.zst* /out/install.sh && cp -L /tmp/release/* /out/"
  mounts=""
fi
podman run --rm --name semu-release-build --platform linux/amd64 --privileged \
  -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix -v "$cache/source:/src:ro" -v "$cache/out:/out" $mounts \
  -e NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0" \
  docker.io/nixos/nix:latest sh -c "git config --global --add safe.directory '*' && $archive $build" \
  || { tail -30 "$cache/out/build.log"; echo "build failed; log: $cache/out/build.log" >&2; exit 1; }
mkdir -p "$here/build"
ln -sfn "$cache/out" "$here/build/release"
if [ -n "$have" ]; then
  ls -lh "$cache/out"/Semu-x86_64.delta.tar.zst
  echo "$(wc -l < "$cache/out/delta-paths") new store paths; next: DECK_HOST=deck@steamdeck.local tests/deck/deploy.sh install-delta"
else
  ls -lh "$cache/out"/Semu-x86_64.tar.zst*
  echo "next: DECK_HOST=deck@steamdeck.local tests/deck/deploy.sh install"
fi
