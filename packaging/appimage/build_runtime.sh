#!/usr/bin/env sh
# Bootstrap the native BTRC runtime builder. Durable build policy lives in
# src/generators/appimage/runtime_builder.btrc.

set -eu

here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
repo="$(CDPATH= cd -- "$here/../.." && pwd -P)"
cache_root="${SEMU_APPIMAGE_CACHE_ROOT:-$HOME/.cache/semu/appimage-runtime}"
tools="$cache_root/tools"
builder="$tools/runtime-builder"
source="$repo/src/generators/appimage/runtime_builder.btrc"
entrypoint="$repo/src/generators/appimage/runtime_builder_main.btrc"
output="${1:-$cache_root/runtime-root}"

mkdir -p "$tools"
if [ ! -x "$builder" ] || [ "$source" -nt "$builder" ] \
    || [ "$entrypoint" -nt "$builder" ] || [ "$repo/flake.lock" -nt "$builder" ]; then
  command -v nix >/dev/null 2>&1 || {
    echo 'Semu AppImage runtime: Nix is required.' >&2
    exit 2
  }
  command -v cc >/dev/null 2>&1 || {
    echo 'Semu AppImage runtime: a native C compiler is required.' >&2
    exit 2
  }
  temporary="$tools/runtime-builder.$$"
  trap 'rm -f -- "$temporary" "$temporary.c"' EXIT HUP INT TERM
  (cd "$repo" && nix run path:.#btrcpy -- "$entrypoint" -o "$temporary.c" \
    --strict-imports --no-cache --no-stdlib)
  cc "$temporary.c" -std=c11 -o "$temporary" -lm
  chmod 0555 "$temporary"
  mv -f "$temporary" "$builder"
  rm -f "$temporary.c"
  trap - EXIT HUP INT TERM
fi

if [ "${SEMU_APPIMAGE_RUNTIME_PLAN:-0}" = 1 ]; then set -- --plan; else set --; fi
exec "$builder" --repo "$repo" --output "$output" "$@"
