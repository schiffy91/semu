#!/usr/bin/env bash
# Cross-compile the Semu GL frame tap for the Steam Deck (x86_64 glibc, the flatpak runtime).
# Output defaults to generated/build/steamdeck/tap/libsemutap.so.
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../../.." && pwd)"
out="${SEMU_TAP_OUT:-$repo_root/generated/build/steamdeck/tap/libsemutap.so}"
mkdir -p "$(dirname "$out")"
cd "$script_dir"
nix run nixpkgs#zig -- cc -target x86_64-linux-gnu -shared -fPIC -O2 -o "$out" libsemutap.c -ldl -lm
echo "built $out"
