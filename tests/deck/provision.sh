#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${1:-$HOME/semu}"
ROMS_DIR="${SEMU_ROMS_DIR:-$PROJECT_DIR/ES-DE/ES-DE/ROMs}"

cd "$PROJECT_DIR"

if command -v pacman >/dev/null 2>&1; then
  sudo pacman -Sy --needed --noconfirm \
    bash curl jq rsync syncthing flatpak bubblewrap retroarch evtest gcc \
    || true
fi

if command -v rpm-ostree >/dev/null 2>&1; then
  rpm-ostree install -y curl jq syncthing flatpak bubblewrap evtest gcc || true
fi

if command -v flatpak >/dev/null 2>&1; then
  flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo || true
fi

mkdir -p "$PROJECT_DIR/build"
if command -v cc >/dev/null 2>&1 && [ -f "$PROJECT_DIR/generated/semu.c" ]; then
  cc "$PROJECT_DIR/generated/semu.c" -std=c11 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -o "$PROJECT_DIR/build/semu" -lm
fi

export SEMU_BIN="$PROJECT_DIR/build/semu"
"$SEMU_BIN" deck install --project "$PROJECT_DIR" --roms "$ROMS_DIR"
"$SEMU_BIN" sync setup --project "$PROJECT_DIR"
