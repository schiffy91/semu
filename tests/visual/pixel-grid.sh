#!/usr/bin/env bash
# Is every native pixel drawn whole? tests/visual/pixel-grid.sh OUT_DIR [WIDTHxHEIGHT...] [-- CELL...]
# A 160x144 card (a one-pixel border in four colours the shells never use, a one-pixel checker, a colour in each
# corner pixel and a 4x4 marker beside it) goes through the real renderer (render.sh, RENDER_HOST_CARD) for each
# cell, the shader off unless the cell names one; cells default to every Game Boy and Game Boy Color bezel in game
# and bezel placement. Each picture is found by its top row, its step k read from that row's height, and the
# 160k x 144k rectangle compared with the card scaled k x k nearest: AE is the device pixels that differ (0: every
# native pixel k by k, none covered, tinted or shifted). Prints one TSV row per picture to OUT_DIR/pixel-grid.tsv
# (picture, k, x, y, margins left right top bottom, AE) and exits non-zero when a picture is missing or any AE > 0.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="$1"; shift
sizes=(); cells=()
while [ $# -gt 0 ] && [ "$1" != "--" ]; do sizes+=("$1"); shift; done
[ $# -gt 0 ] && shift
cells=("$@")
[ ${#sizes[@]} -gt 0 ] || sizes=(1280x800 1920x1080)
[ ${#cells[@]} -gt 0 ] || cells=(gb:dmg:none 'gb:dmg:none>@bezel' gb:studio:none 'gb:studio:none>@bezel' gbc:shell:none 'gbc:shell:none>@bezel' gbc:berry:none 'gbc:berry:none>@bezel')
mkdir -p "$out"
scratch="$(mktemp -d)"  # the card and the comparisons; left behind, never removed
card="$scratch/card.png"
magick -size 160x144 xc: -fx '((i+j)%2)*0.70+0.15' -type TrueColor \
  -fill 'rgb(211,7,113)' -draw 'line 1,0 158,0' -fill 'rgb(7,211,113)' -draw 'line 159,1 159,142' \
  -fill 'rgb(113,7,211)' -draw 'line 1,143 158,143' -fill 'rgb(211,113,7)' -draw 'line 0,1 0,142' \
  -fill 'rgb(241,3,241)' -draw 'point 0,0' -draw 'rectangle 2,2 5,5' -fill 'rgb(3,241,241)' -draw 'point 159,0' -draw 'rectangle 154,2 157,5' \
  -fill 'rgb(241,131,3)' -draw 'point 0,143' -draw 'rectangle 2,138 5,141' -fill 'rgb(251,251,251)' -draw 'point 159,143' -draw 'rectangle 154,138 157,141' \
  "PNG24:$card"
failures=0
printf 'picture\tk\tx\ty\tleft\tright\ttop\tbottom\tAE\n' > "$out/pixel-grid.tsv"
for size in "${sizes[@]}"; do
  RENDER_HOST_CARD="$card" "$root/tests/visual/render.sh" "$out/$size" "$size" "${cells[@]}" >/dev/null || failures=$((failures + 1))
  for picture in "$out/$size"/*.png; do
    case "$picture" in *-before.png|*-toast.png|*.expected.png|*.crop.png|*.diff.png) continue ;; esac
    box="$(magick "$picture" -alpha off -fuzz 0 -fill black +opaque 'rgb(211,7,113)' -fill white -opaque 'rgb(211,7,113)' -format '%@' info:)"
    read -r rowWidth step rowX rowY <<<"$(echo "$box" | sed -E 's/^([0-9]+)x([0-9]+)\+(-?[0-9]+)\+(-?[0-9]+)$/\1 \2 \3 \4/')"
    if [ "$step" -lt 1 ] 2>/dev/null || [ "$step" -gt 32 ] || [ "$rowWidth" -ne $((158 * step)) ]; then
      printf '%s\tmissing\t\t\t\t\t\t\t%s\n' "$size/$(basename "$picture")" "$box" >> "$out/pixel-grid.tsv"; failures=$((failures + 1)); continue
    fi
    left=$((rowX - step)); top=$rowY; width=$((160 * step)); height=$((144 * step))
    screenWidth="${size%x*}"; screenHeight="${size#*x}"
    magick "$card" -sample "${width}x${height}!" "${picture%.png}.expected.png"
    magick "$picture" -alpha off -crop "${width}x${height}+${left}+${top}" +repage "${picture%.png}.crop.png"
    differing="$(magick compare -metric AE "${picture%.png}.expected.png" "${picture%.png}.crop.png" "${picture%.png}.diff.png" 2>&1 || true)"
    differing="${differing%% *}"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$size/$(basename "$picture")" "$step" "$left" "$top" "$left" $((screenWidth - left - width)) "$top" $((screenHeight - top - height)) "$differing" >> "$out/pixel-grid.tsv"
    [ "$differing" = "0" ] || failures=$((failures + 1))
  done
done
column -t -s $'\t' "$out/pixel-grid.tsv"
exit "$failures"
