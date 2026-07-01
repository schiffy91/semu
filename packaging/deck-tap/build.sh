#!/usr/bin/env bash
# Cross-compile the Semu GL frame tap for the Steam Deck (x86_64 glibc, the flatpak runtime).
# Output: libsemutap.so — LD_PRELOAD'd into RetroArch by launcher.btrc (launcherTapLib).
set -euo pipefail
cd "$(dirname "$0")"
nix run nixpkgs#zig -- cc -target x86_64-linux-gnu -shared -fPIC -O2 -o libsemutap.so libsemutap.c -ldl -lm
echo "built $(pwd)/libsemutap.so"
