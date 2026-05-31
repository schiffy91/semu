#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${1:-$HOME/schemulator}"
SCHEM="${SCHEMULATOR_BIN:-$PROJECT_DIR/build/schemulator}"
STRICT="${SCHEMULATOR_STRICT_INPUT:-0}"

"$SCHEM" doctor --project "$PROJECT_DIR" | grep -F 'OK hotkeys: HKB+L1 load, HKB+R1 save, HKB+Start quit'
"$SCHEM" doctor --project "$PROJECT_DIR" | grep -F 'OK right_trackpad: mouse'
"$SCHEM" doctor --project "$PROJECT_DIR" | grep -F 'OK left_trackpad: radial_hotkeys'

if [ -e /dev/uinput ]; then
  echo "OK /dev/uinput"
elif [ "$STRICT" = "1" ]; then
  echo "FAIL /dev/uinput missing; virtual-input automation unavailable" >&2
  exit 2
else
  echo "WARN /dev/uinput missing; virtual-input automation unavailable"
fi

if command -v inputplumber >/dev/null 2>&1; then
  echo "OK inputplumber"
elif [ "$STRICT" = "1" ]; then
  echo "FAIL inputplumber missing; Steam Deck route cannot be verified" >&2
  exit 2
else
  echo "WARN inputplumber missing; Steam Deck Game Mode still needs physical pass"
fi

echo "OK deck input preflight"
