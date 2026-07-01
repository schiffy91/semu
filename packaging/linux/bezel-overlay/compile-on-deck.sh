#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
export NIX_PATH=nixpkgs=flake:nixpkgs
nix-shell -p gcc pkg-config cairo xorg.libX11 xorg.libXext libglvnd --run \
  'gcc semu-bezel-overlay.c -o semu-bezel-overlay $(pkg-config --cflags --libs cairo x11 xext) -lGL'
echo "built: $(ls -la semu-bezel-overlay 2>/dev/null)"
echo "=== ldd missing? ==="
ldd semu-bezel-overlay 2>/dev/null | grep -i "not found" || echo "all libs resolved"
