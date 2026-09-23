#!/usr/bin/env bash
# A compositor edit mid-game must change nothing but the program: render, edit, render, compare (macOS render host).
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
make -C "$root" --no-print-directory build/semu build/render-host >/dev/null
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cp -R "$root/config/render" "$work/compositor"
while IFS= read -r line; do export "$line"; done < <("$root/build/semu" render-env --system "${1:-gba}" --project "$root/config" --asset-root "$root/build/asset-stub")
export SEMU_RENDER_COMPOSITOR_DIR="$work/compositor"
"$root/build/render-host" 1280 800 "$work/before.ppm" --reload "$work/after.ppm" 2>"$work/log"
grep -q "compositor shader reloaded" "$work/log" || { echo "hot reload: the edit was never picked up"; cat "$work/log"; exit 1; }
changed="$(magick compare -metric AE "$work/before.ppm" "$work/after.ppm" null: 2>&1 | cut -d' ' -f1 || true)"
[ "$changed" = "0" ] || { echo "hot reload: $changed pixels changed after the reload"; exit 1; }
echo "hot reload: the reloaded program draws the same frame"
