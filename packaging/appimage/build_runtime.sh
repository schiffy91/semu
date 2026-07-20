#!/usr/bin/env sh
# Bootstrap the native BTRC runtime builder. Durable build policy lives in
# src/generators/appimage/runtime_builder.btrc.

set -eu

here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
repo="$(CDPATH= cd -- "$here/../.." && pwd -P)"
cache_root="${SEMU_APPIMAGE_CACHE_ROOT:-$HOME/.cache/semu/appimage-runtime}"
source="$repo/src/generators/appimage/runtime_builder.btrc"
entrypoint="$repo/src/generators/appimage/runtime_builder_main.btrc"
output="${1:-$cache_root/runtime-root}"

command -v nix >/dev/null 2>&1 || {
  echo 'Semu AppImage runtime: Nix is required.' >&2
  exit 2
}
command -v cc >/dev/null 2>&1 || {
  echo 'Semu AppImage runtime: a native C compiler is required.' >&2
  exit 2
}

bootstrap="$(mktemp -d "${TMPDIR:-/tmp}/semu-runtime-builder.XXXXXX")"
builder="$bootstrap/runtime-builder"
trap 'rm -rf -- "$bootstrap"' EXIT HUP INT TERM
(cd "$repo" && nix run .#btrcpy -- "$entrypoint" -o "$builder.c" \
  --strict-imports --no-cache --no-stdlib)
cc "$builder.c" -std=c11 -o "$builder" -lm

if [ "${SEMU_APPIMAGE_RUNTIME_PLAN:-0}" = 1 ]; then set -- --plan; else set --; fi
"$builder" --repo "$repo" --output "$output" "$@"
