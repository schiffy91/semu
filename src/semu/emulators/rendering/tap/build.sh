#!/usr/bin/env bash
# build.sh — host build path for the tap's compiled artifacts.
#
# 1. Compile + run the host contract checks (tap_geometry_check/tap_menu_check)
#    so the shared pure-math contracts stay proven before anything reuses them.
# 2. On darwin, compile the macOS overlay compositor (macos_overlay.m) to
#    src/generated/build/macos/tap/semu-overlay — the binary
#    platforms/macos/macos_tap.btrc resolves at launch time.
#
# The Deck's libsemutap.so is NOT built here: that stays with the semu-tap
# stanza in src/semu/packaging/nix/flake/packages.nix (x86_64-linux toolchain).
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../../../.." && pwd)"
check_dir="$repo_root/src/generated/test"
overlay_out="${SEMU_OVERLAY_OUT:-$repo_root/src/generated/build/macos/tap/semu-overlay}"
mkdir -p "$check_dir"
cd "$script_dir"

cc tap_geometry_check.c -std=c11 -o "$check_dir/tap-geometry-check" -lm
"$check_dir/tap-geometry-check"
cc tap_menu_check.c -std=c11 -o "$check_dir/tap-menu-check" -lm
"$check_dir/tap-menu-check"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "skip semu-overlay: darwin-only (host $(uname -s))"
  exit 0
fi
mkdir -p "$(dirname "$overlay_out")"
clang -fobjc-arc -Wall -O2 -framework Cocoa -framework CoreGraphics \
  -o "$overlay_out" macos_overlay.m
echo "built $overlay_out"
