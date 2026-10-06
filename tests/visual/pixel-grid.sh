#!/usr/bin/env bash
# Is every native pixel drawn whole, with the shader off and on? tests/visual/pixel-grid.sh OUT_DIR [WIDTHxHEIGHT...] [-- CELL...]
# A CELL is a render.sh cell, system:bezel[:shader][>@placement]. Each screen is fed a card at its own native size (the second
# through RENDER_HOST_CARD_1): a one-pixel checker inside a coloured border drawn four pixels deep, one colour per side and per
# corner and another set on the second screen, colours no shell uses. Every cell is drawn with the shader off first (its shader
# replaced by none): each picture is found by its top border, its step k read from that border's height (4k), and the picture
# compared with the card scaled k by k nearest. whole is the device pixels that differ (0: every native pixel k by k, none
# covered, tinted or shifted). Unless the cell's shader is none it is drawn again with that shader, and on each side the outermost
# k-pixel strip is compared with the strip one native pixel in. The card gives the two the same colour and the same neighbours
# once the picture's edge carries on (clamp to edge), so a whole, undarkened edge pixel draws as its twin; a shader that lights
# the edge from black past it does not. edge_vs_twin is the pixels that differ at all (a look drawn over a paper, or one whose
# sub-pixels alias at 1x, differs by a level or two, or in hue, from place to place); darker the pixels whose light (Rec. 709
# luma) is more than three levels below their twin's, the measure: 0; outer_ratio the outermost line's mean light over its
# twin line's, the lowest side. A look named in PIXEL_GRID_RESAMPLED (default agb001) is reported, not failed, on darker: its
# last pass scales a 4x sub-pixel pattern to the screen with a linear filter, so at a step that is not even each pixel's
# outer line borrows a little of its neighbour's sub-pixel, and the picture's outermost line, with no neighbour past it,
# shows its own (clamp to edge) and differs from its twin by that blur. PIXEL_GRID_BASELINE=<asset root> also draws each
# shaded cell from that bundle: interior counts the differing pixels inside the outermost ring of native pixels, picture
# those in the whole picture. PIXEL_GRID_ASSET_ROOT=<asset root> measures another bundle (the launch's, build/asset-root, by
# default). Cells default to every Game Boy and Game Boy Color bezel with the shader off, and every look of the Game Boy, Game
# Boy Color, Game Boy Advance, DS and 3DS on its default bezel in game and bezel placement, the DS and 3DS also beside a large
# main. One TSV row per screen picture goes to OUT_DIR/pixel-grid.tsv; the exit status is non-zero when a picture is missing
# or any whole, darker or interior count is not 0.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
out="$1"; shift
sizes=(); cells=()
while [ $# -gt 0 ] && [ "$1" != "--" ]; do sizes+=("$1"); shift; done
[ $# -gt 0 ] && shift
cells=("$@")
[ ${#sizes[@]} -gt 0 ] || sizes=(1280x800 1920x1080)
if [ ${#cells[@]} -eq 0 ]; then
  cells=(gb:dmg:none 'gb:dmg:none>@bezel' gbc:shell:none 'gbc:shell:none>@bezel' gbc:berry:none 'gbc:berry:none>@bezel')
  for system in gb gbc gba nds n3ds; do
    bezel="$(jq -r '.default_variant' "$root/config/systems/$system/bezels.json")"
    for look in $(jq -r '.variants[].id' "$root/config/systems/$system/shaders.json"); do
      cells+=("$system:$bezel:$look" "$system:$bezel:$look>@bezel")
      case "$system" in nds|n3ds) cells+=("$system:main_right:$look") ;; esac
    done
  done
fi
mkdir -p "$out"
scratch="$(mktemp -d)"  # the cards, crops and per-cell renders; left behind, never removed

palette() {  # SCREEN: the border colours top right bottom left, then the corners top-left top-right bottom-left bottom-right
  if [ "$1" = 0 ]; then echo 'rgb(211,7,113) rgb(7,211,113) rgb(113,7,211) rgb(211,113,7) rgb(241,3,241) rgb(3,241,241) rgb(241,131,3) rgb(251,251,251)'
  else echo 'rgb(17,97,203) rgb(97,203,17) rgb(203,17,97) rgb(203,97,17) rgb(233,11,233) rgb(11,233,233) rgb(233,139,11) rgb(245,245,245)'; fi
}

card() {  # WIDTH HEIGHT SCREEN OUT.png: the checker inside a border four native pixels deep
  local width="$1" height="$2" top right bottom left topLeft topRight bottomLeft bottomRight
  read -r top right bottom left topLeft topRight bottomLeft bottomRight <<<"$(palette "$3")"
  magick -size "${width}x${height}" xc: -fx '((i+j)%2)*0.70+0.15' -type TrueColor +antialias \
    -fill "$top" -draw "rectangle 4,0 $((width - 5)),3" -fill "$right" -draw "rectangle $((width - 4)),4 $((width - 1)),$((height - 5))" \
    -fill "$bottom" -draw "rectangle 4,$((height - 4)) $((width - 5)),$((height - 1))" -fill "$left" -draw "rectangle 0,4 3,$((height - 5))" \
    -fill "$topLeft" -draw 'rectangle 0,0 3,3' -fill "$topRight" -draw "rectangle $((width - 4)),0 $((width - 1)),3" \
    -fill "$bottomLeft" -draw "rectangle 0,$((height - 4)) 3,$((height - 1))" -fill "$bottomRight" -draw "rectangle $((width - 4)),$((height - 4)) $((width - 1)),$((height - 1))" \
    "PNG24:$4"
}

draw() {  # SIZE CELL DIR [ASSET_ROOT]: render.sh into DIR; the picture it ends on (a switch's -after)
  local system="${2%%:*}" picture
  mkdir -p "$3"
  RENDER_HOST_CARD="$scratch/$system-0.png" RENDER_HOST_CARD_1="$scratch/$system-1.png" SEMU_ASSET_ROOT="${4:-${PIXEL_GRID_ASSET_ROOT:-$root/build/asset-root}}" \
    "$root/tests/visual/render.sh" "$3" "$1" "$2" >/dev/null || return 1
  for picture in "$3"/*.png; do case "$picture" in *-before.png|*-toast.png) ;; *) echo "$picture"; return 0 ;; esac; done
  return 1
}

differing() {  # PICTURE GEOMETRY OTHER GEOMETRY [OTHER_PICTURE]: pixels that differ between two crops
  local result
  result="$(magick compare -metric AE <(magick "$1" -alpha off -crop "$2" +repage PNG24:-) <(magick "${4:-$1}" -alpha off -crop "$3" +repage PNG24:-) null: 2>&1 || true)"
  echo "${result%% *}"
}

light() { magick "$1" -alpha off -crop "$2" +repage -format '%[fx:mean]' info:; }  # PICTURE GEOMETRY: mean light

darker() {  # PICTURE EDGE TWIN: the pixels of the edge strip whose light (Rec. 709 luma) is more than three levels below their twin's
  magick \( "$1" -alpha off -crop "$3" +repage -grayscale Rec709Luma \) \( "$1" -alpha off -crop "$2" +repage -grayscale Rec709Luma \) -fx 'u-v' -threshold 1.2% -format '%[fx:int(mean*w*h+0.5)]' info:
}

failures=0
resampled=" ${PIXEL_GRID_RESAMPLED-agb001} "  # looks whose outermost line differs from its twin by their own blur, reported not failed
printf 'picture\tscreen\tk\tx\ty\twhole\tshader\tedge_vs_twin\tdarker\touter_ratio\tinterior\tpicture_vs_baseline\tnote\n' > "$out/pixel-grid.tsv"
make -C "$root" --no-print-directory build/semu build/render-host >/dev/null
nix build --no-warn-dirty --out-link "$root/build/asset-root" "$root#asset-root"
for size in "${sizes[@]}"; do
  index=0
  for cell in "${cells[@]}"; do
    index=$((index + 1))
    IFS=: read -r system bezel shader <<<"${cell%%>*}"
    switching=""; [ "$cell" != "${cell#*>}" ] && switching=">${cell#*>}"
    screens="$(jq '.display.screens | length' "$root/config/systems/$system/system.json")"
    for screen in $(seq 0 $((screens - 1))); do
      [ -f "$scratch/$system-$screen.png" ] || card "$(jq ".display.screens[$screen].native.w" "$root/config/systems/$system/system.json")" \
        "$(jq ".display.screens[$screen].native.h" "$root/config/systems/$system/system.json")" "$screen" "$scratch/$system-$screen.png"
    done
    [ "$screens" = 2 ] || cp "$scratch/$system-0.png" "$scratch/$system-1.png"
    label="$size/$index-${cell//[>:@]/-}"
    plain="$(draw "$size" "$system:$bezel:none$switching" "$out/$size/$index-off")" || { printf '%s\tmissing\n' "$label" >> "$out/pixel-grid.tsv"; failures=$((failures + 1)); continue; }
    shaded=""; baseline=""
    if [ "$shader" != none ]; then  # a cell naming no shader draws the launch's own look
      shaded="$(draw "$size" "$cell" "$out/$size/$index-on")" || { printf '%s\tmissing shaded\n' "$label" >> "$out/pixel-grid.tsv"; failures=$((failures + 1)); continue; }
      [ -z "${PIXEL_GRID_BASELINE:-}" ] || baseline="$(draw "$size" "$cell" "$out/$size/$index-baseline" "$PIXEL_GRID_BASELINE")" || baseline=""
    fi
    for screen in $(seq 0 $((screens - 1))); do
      card="$scratch/$system-$screen.png"
      width="$(magick identify -format '%w' "$card")"; height="$(magick identify -format '%h' "$card")"
      top="$(palette "$screen")"; top="${top%% *}"
      box="$(magick "$plain" -alpha off -fuzz 0 -fill black +opaque "$top" -fill white -opaque "$top" -format '%@' info:)"
      read -r bandWidth bandHeight bandX bandY <<<"$(echo "$box" | sed -E 's/^([0-9]+)x([0-9]+)\+(-?[0-9]+)\+(-?[0-9]+)$/\1 \2 \3 \4/')"
      step=$((bandHeight / 4))
      if [ "$step" -lt 1 ] || [ "$bandHeight" -ne $((4 * step)) ] || [ "$bandWidth" -ne $(((width - 8) * step)) ]; then
        printf '%s\t%s\tmissing\t%s\n' "$label" "$screen" "$box" >> "$out/pixel-grid.tsv"; failures=$((failures + 1)); continue
      fi
      left=$((bandX - 4 * step)); upper=$bandY; pictureWidth=$((width * step)); pictureHeight=$((height * step))
      rectangle="${pictureWidth}x${pictureHeight}+${left}+${upper}"
      magick "$card" -sample "${pictureWidth}x${pictureHeight}!" "$scratch/expected.png"
      whole="$(differing "$scratch/expected.png" "${pictureWidth}x${pictureHeight}+0+0" "$rectangle" "$plain")"
      [ "$whole" = 0 ] || failures=$((failures + 1))
      exact=""; darkened=""; ratio=""; interior=""; overall=""; note=""
      if [ -n "$shaded" ]; then
        column="${step}x${pictureHeight}"; row="${pictureWidth}x${step}"
        strips=("$column+$left+$upper $column+$((left + step))+$upper" "$column+$((left + (width - 1) * step))+$upper $column+$((left + (width - 2) * step))+$upper"
                "$row+$left+$upper $row+$left+$((upper + step))" "$row+$left+$((upper + (height - 1) * step)) $row+$left+$((upper + (height - 2) * step))")
        lines=("1x$pictureHeight+$left+$upper 1x$pictureHeight+$((left + step))+$upper" "1x$pictureHeight+$((left + pictureWidth - 1))+$upper 1x$pictureHeight+$((left + pictureWidth - step - 1))+$upper"
               "${pictureWidth}x1+$left+$upper ${pictureWidth}x1+$left+$((upper + step))" "${pictureWidth}x1+$left+$((upper + pictureHeight - 1)) ${pictureWidth}x1+$left+$((upper + pictureHeight - step - 1))")
        exact=0; darkened=0; ratio=1
        for side in 0 1 2 3; do
          read -r edge twin <<<"${strips[$side]}"
          exact=$((exact + $(differing "$shaded" "$edge" "$twin")))
          darkened=$((darkened + $(darker "$shaded" "$edge" "$twin")))
          read -r edge twin <<<"${lines[$side]}"
          ratio="$(awk -v edge="$(light "$shaded" "$edge")" -v twin="$(light "$shaded" "$twin")" -v kept="$ratio" 'BEGIN { value = (twin > 0) ? edge / twin : 1; if (kept + 0 < value) value = kept; printf "%.4f", value }')"
        done
        note=""; case "$resampled" in *" ${shader:-default} "*) [ "$darkened" = 0 ] || note=resampled ;; esac
        [ "$darkened" = 0 ] || [ -n "$note" ] || failures=$((failures + 1))
        if [ -n "$baseline" ]; then
          inner="$((pictureWidth - 2 * step))x$((pictureHeight - 2 * step))+$((left + step))+$((upper + step))"
          interior="$(differing "$shaded" "$inner" "$inner" "$baseline")"
          overall="$(differing "$shaded" "$rectangle" "$rectangle" "$baseline")"
          [ "$interior" = 0 ] || failures=$((failures + 1))
        fi
      fi
      printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$label" "$screen" "$step" "$left" "$upper" "$whole" "${shader:-default}" \
        "$exact" "$darkened" "$ratio" "$interior" "$overall" "$note" >> "$out/pixel-grid.tsv"
    done
  done
done
column -t -s $'\t' "$out/pixel-grid.tsv"
exit "$failures"
