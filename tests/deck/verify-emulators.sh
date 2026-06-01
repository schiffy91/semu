#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${1:-$HOME/semu}"
SEMU="${SEMU_BIN:-$PROJECT_DIR/build/semu}"

"$SEMU" doctor --project "$PROJECT_DIR"
"$SEMU" keymap validate --project "$PROJECT_DIR"
"$SEMU" keymap render --project "$PROJECT_DIR" --target retroarch >/tmp/semu-retroarch.cfg
"$SEMU" screenshot status --project "$PROJECT_DIR"
if { [ -n "${WAYLAND_DISPLAY:-}" ] || [ -n "${DISPLAY:-}" ]; } && \
   { command -v grim >/dev/null 2>&1 || command -v spectacle >/dev/null 2>&1 || command -v gnome-screenshot >/dev/null 2>&1 || command -v import >/dev/null 2>&1; }; then
  "$SEMU" screenshot capture --project "$PROJECT_DIR" --emulator deck_preflight --hook manual_visual_checkpoint
else
  echo "WARN screenshot capture skipped: no graphical session or screenshot tool"
fi
"$SEMU" e2e sandbox
"$SEMU" e2e launcher

command -v retroarch >/dev/null 2>&1 && retroarch --version | head -1
command -v flatpak >/dev/null 2>&1 && flatpak list --app || true

echo "OK deck emulator preflight"
