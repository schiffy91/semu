#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${1:-$HOME/semu}"
SCHEM="${SEMU_BIN:-$PROJECT_DIR/build/semu}"

"$SCHEM" doctor --project "$PROJECT_DIR"
"$SCHEM" keymap validate --project "$PROJECT_DIR"
"$SCHEM" keymap render --project "$PROJECT_DIR" --target retroarch >/tmp/semu-retroarch.cfg
"$SCHEM" screenshot status --project "$PROJECT_DIR"
if { [ -n "${WAYLAND_DISPLAY:-}" ] || [ -n "${DISPLAY:-}" ]; } && \
   { command -v grim >/dev/null 2>&1 || command -v spectacle >/dev/null 2>&1 || command -v gnome-screenshot >/dev/null 2>&1 || command -v import >/dev/null 2>&1; }; then
  "$SCHEM" screenshot capture --project "$PROJECT_DIR" --emulator deck_preflight --hook manual_visual_checkpoint
else
  echo "WARN screenshot capture skipped: no graphical session or screenshot tool"
fi
"$SCHEM" e2e sandbox
"$SCHEM" e2e launcher

command -v retroarch >/dev/null 2>&1 && retroarch --version | head -1
command -v flatpak >/dev/null 2>&1 && flatpak list --app || true

echo "OK deck emulator preflight"
