#!/usr/bin/env bash
# The real renderer on this Mac, no emulator: tests/visual/render.sh OUT_DIR WIDTHxHEIGHT system[:bezel_variant[:shader_variant]]...
# Each cell is `semu render-env` for that choice fed to build/render-host; the PPM becomes OUT_DIR/<system>-<variant>.png.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="$1"; size="$2"; shift 2
width="${size%x*}"; height="${size#*x}"
make -C "$root" --no-print-directory build/semu build/render-host build/bezel-tree >/dev/null
mkdir -p "$out" "$root/build/asset-stub"
export SEMU_BEZEL_TREE="$root/build/bezel-tree/share/semu/bezel/shaders"
failures=0
for cell in "$@"; do
  IFS=: read -r system bezel shader <<<"$cell"
  settings="{\"visual\":{\"systems\":{\"$system\":{${bezel:+\"bezel_variant\":\"$bezel\"}${bezel:+${shader:+,}}${shader:+\"shader_variant\":\"$shader\"}}}}}"
  name="$system${bezel:+-$bezel}${shader:+-$shader}"
  (
    while IFS= read -r line; do export "$line"; done < <("$root/build/semu" render-env --system "$system" --project "$root/config" \
      --asset-root "${SEMU_ASSET_ROOT:-$root/build/asset-stub}" --settings-json "$settings")
    "$root/build/render-host" "$width" "$height" "$out/$name.ppm"
  ) 2>"$out/$name.log" && magick "$out/$name.ppm" "PNG24:$out/$name.png" && rm -f "$out/$name.ppm" \
    && echo "$name: $out/$name.png" || { echo "$name: FAILED (see $out/$name.log)"; failures=$((failures + 1)); }
done
exit "$failures"
