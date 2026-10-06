#!/usr/bin/env bash
# The picture edge in every layered bezel, from the real renderer on this Mac: the lip covers everything from the bent
# picture edge to the plastic, so the screen's surround colour never reaches the frame. Each layered fixed variant lights
# a flat white card (shader off) twice, both screens' surround magenta and then black, at the Deck (1280x800) and 4K
# (3840x2160); a pixel that differs is surround showing through, the dark stair-step seam along a curved picture edge.
# usage: edge-seam.sh [--quick] [system...]   --quick checks the Deck size only.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
quick=0; [ "${1:-}" = "--quick" ] && { quick=1; shift; }
sizes="1280x800 3840x2160"; [ "$quick" = 1 ] && sizes="1280x800"
make -C "$root" --no-print-directory build/semu build/render-host >/dev/null
nix build --no-warn-dirty --out-link "$root/build/asset-root" "$root#asset-root"
semu="$root/build/semu"
host="$root/build/render-host"
systems=("$@")
[ ${#systems[@]} -gt 0 ] || mapfile -t systems < <(cd "$root/config/systems" && for s in *; do jq -e '.variants | length > 0' "$s/bezels.json" >/dev/null 2>&1 && echo "$s"; done)
work="$(mktemp -d)"  # the renders and their logs, left behind, never removed
failures=0
checked=0

environment() {  # SYSTEM VARIANT: the launcher's render environment for that variant, its shader off so the card stays flat
  "$semu" render-env --system "$1" --project "$root/config" --asset-root "$root/build/asset-root" \
    --settings-json "{\"visual\":{\"systems\":{\"$1\":{\"bezel_variant\":\"$2\",\"shader_variant\":\"none\"}}}}"
}

render() {  # ENVIRONMENT WIDTH HEIGHT SURROUND OUT.ppm: the white card with both screens' surround painted SURROUND
  ( while IFS= read -r line; do export "$line"; done < "$1"
    SEMU_RENDER_SCREEN_0_SURROUND="$4" SEMU_RENDER_SCREEN_1_SURROUND="$4" RENDER_HOST_CARD=white "$host" "$2" "$3" "$5" ) 2>>"$work/render.log"
}

for system in "${systems[@]}"; do
  for variant in $(jq -r '.variants[].id' "$root/config/systems/$system/bezels.json"); do
    environment "$system" "$variant" > "$work/environment"
    layers="$(sed -n 's/^SEMU_RENDER_LAYER_COUNT=//p' "$work/environment")"
    grep -q '^SEMU_RENDER_LAYOUT=fixed$' "$work/environment" && grep -q '^SEMU_RENDER_SCREEN_0_RING=' "$work/environment" \
      && [ "${layers:-0}" -gt 0 ] || continue  # only a layered package starts its lip at the bent picture edge
    for size in $sizes; do
      width="${size%x*}"; height="${size#*x}"
      render "$work/environment" "$width" "$height" 1,0,1 "$work/magenta.ppm" & magenta=$!
      render "$work/environment" "$width" "$height" 0,0,0 "$work/black.ppm" & black=$!
      rendered=1; wait "$magenta" || rendered=0; wait "$black" || rendered=0
      if [ "$rendered" = 0 ]; then
        echo "$system/$variant $size: render failed"; tail -n 5 "$work/render.log"; failures=$((failures + 1)); continue
      fi
      leaked="$(magick "$work/magenta.ppm" "$work/black.ppm" -compose difference -composite -separate -evaluate-sequence max \
        -threshold 0 -format '%[fx:round(mean*w*h)]' info:)"
      verdict="pass"; [ "$leaked" = 0 ] || { verdict="FAIL"; failures=$((failures + 1)); }
      checked=$((checked + 1))
      echo "$system/$variant $size: $leaked px show the surround ($verdict)"
    done
  done
done
echo "edge-seam: $checked checked, $failures failing"
exit "$failures"
