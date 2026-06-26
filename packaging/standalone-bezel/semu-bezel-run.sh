#!/usr/bin/env bash
# semu-bezel-run.sh <4x3|16x9> <emulator command...>
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
ASPECT="${1:-4x3}"; shift || true
OVL="$DIR/semu-bezel-overlay"; ASSET="$DIR/assets/bezel-$ASPECT.png"
if [ -x "$OVL" ] && [ -f "$ASSET" ]; then
  "$OVL" "$ASSET" >>"${SEMU_BEZEL_LOG:-/tmp/semu-bezel-overlay.log}" 2>&1 & OVLPID=$!
else
  echo "semu-bezel: overlay/asset missing ($OVL / $ASSET) - running without bezel" >&2
fi
"$@"
[ -n "${OVLPID:-}" ] && kill "$OVLPID" 2>/dev/null || true
