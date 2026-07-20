#!/usr/bin/env sh
set -eu

repo=${1:?repository root is required}
source_c=${2:?transpiled installer contract is required}
stage="$(mktemp -d "${TMPDIR:-/tmp}/semu-installer-contract.XXXXXX")"
bootstrap_tmp=""
trap 'rm -rf -- "$stage"; [ -z "$bootstrap_tmp" ] || rm -rf -- "$bootstrap_tmp"' EXIT HUP INT TERM
binary="$stage/semu-installer-contract"

if [ "$(uname -s)" = Linux ]; then
  bootstrap_tmp="/dev/shm/semu-installer-bootstrap.$$"
  mkdir -m 0700 -- "$bootstrap_tmp"
  cc "$source_c" -std=c11 -o "$binary" -lm
  SEMU_TEST_BOOTSTRAP_TMPDIR="$bootstrap_tmp" \
    "$binary" --project "$repo"
  exit
fi

case "$(uname -m)" in
  arm64|aarch64) target=aarch64-linux-musl; platform=linux/arm64 ;;
  *) target=x86_64-linux-musl; platform=linux/amd64 ;;
esac
nix shell nixpkgs#zig --command zig cc -target "$target" \
  -std=c11 "$source_c" -o "$binary" -lm
mkdir -p "$stage/packaging/install" "$stage/packaging/appimage"
cp "$repo/packaging/install/installer.sh" \
  "$repo/packaging/install/legacy_shortcuts.json" \
  "$stage/packaging/install/"
cp "$repo/packaging/appimage/semu-launcher.template" \
  "$repo/packaging/appimage/semu.desktop.template" \
  "$stage/packaging/appimage/"
cp -R "$repo/config" "$stage/config"
podman run --rm --platform "$platform" \
  --tmpfs /bootstrap-tmp:rw,mode=0700 \
  --env SEMU_TEST_BOOTSTRAP_TMPDIR=/bootstrap-tmp \
  --mount "type=bind,source=$stage,target=/contract,ro" \
  --entrypoint /contract/semu-installer-contract \
  docker.io/library/debian:bookworm-slim --project /contract
