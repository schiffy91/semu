#!/usr/bin/env bash
# gen_bezel_manifest.sh — auto-measure every bezel's TV-glass rect and emit the manifest.
#
# Root-cause fix for the off-center bug: NEVER hand-measure a bezel hole. For each bezel PNG
# this detects the glass programmatically (connected-components on the near-black region),
# normalizes to fractions, and SNAPS the hole to the system's display aspect (center-preserving)
# so the bezel-frame map stays uniform (no distortion) and the centered game fills the glass.
# Handles asymmetric assets automatically — the hole is wherever the glass actually is.
#
# Emits:
#   src/generated/assets/bezels.json         measured manifest (fold into src/semu/systems/<id>/bezels.json)
#
# Overscan is intentionally NOT here — it's a game/core property (per-system default + per-game
# override), not a bezel property.
#
# Usage: gen_bezel_manifest.sh [ASSET_ROOT]
#   ASSET_ROOT = dir holding share/libretro/shaders/Mega_Bezel_Packs/... (the staged bundle assets).
#   PNGs must come from ASSET_ROOT or from the checked-in handheld shell assets.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../../../../.." && pwd)"
ROOT="${1:-}"
IM="$(command -v magick || command -v convert)"
ID="$(command -v identify || echo "$IM identify")"
[ -n "$IM" ] || { echo "ERROR: ImageMagick (magick/convert) not found"; exit 1; }

# system | relative PNG path under <ROOT>/share/libretro/shaders/Mega_Bezel_Packs | display aspect W:H
TABLE=(
  "n64|Soqueroeu-TV-Backgrounds_V2.0/img/Nintendo_N64/N64.png|4:3"
  "nes|Soqueroeu-TV-Backgrounds_V2.0/img/Nintendo_NES/NES.png|4:3"
  "snes|Soqueroeu-TV-Backgrounds_V2.0/img/Nintendo_SuperNintendo/SuperNES.png|4:3"
  "genesis|Soqueroeu-TV-Backgrounds_V2.0/img/Sega_Genesis/Sega_Genesis.png|4:3"
  "psx|Soqueroeu-TV-Backgrounds_V2.0/img/Sony_Playstation/PSX.png|4:3"
  "gb|semu-shells/gb.png|10:9"
  "gbc|semu-shells/gbc.png|10:9"
  "gba|semu-shells/gba.png|3:2"
)

resolve_png() {  # $1=system $2=relpath -> echoes a readable path or empty
  local sys="$1" rel="$2" p
  if [ -n "$ROOT" ]; then
    p="$ROOT/share/libretro/shaders/Mega_Bezel_Packs/$rel"; [ -f "$p" ] && { echo "$p"; return; }
    p="$ROOT/$rel";                                          [ -f "$p" ] && { echo "$p"; return; }
  fi
  echo ""
}

measure_glass() {  # $1=png -> echoes "W H gx gy gw gh" (px) or fails
  local png="$1" dim W H bbox
  dim=$("$IM" "$png" -format "%w %h" info: 2>/dev/null)
  W="${dim% *}"; H="${dim#* }"
  bbox=$("$IM" "$png" -colorspace Gray -threshold 6% -negate \
        -define connected-components:verbose=true \
        -define connected-components:area-threshold=4000 \
        -connected-components 4 null: 2>/dev/null \
      | awk '/gray\(255\)/{for(i=1;i<=NF;i++) if($i ~ /^[0-9]+x[0-9]+\+[0-9]+\+[0-9]+$/){
               split($i,a,/[x+]/); ar=a[1]*a[2];
               if(ar>best){best=ar; bw=a[1]; bh=a[2]; bx=a[3]; by=a[4]} }}
             END{ if(best>0) print bx,by,bw,bh }')
  [ -n "$bbox" ] || return 1
  echo "$W $H $bbox"
}

JSON="$REPO/src/generated/assets/bezels.json"
mkdir -p "$(dirname "$JSON")"
tmpj="$(mktemp)"
echo "{" > "$tmpj"

# Manual hole overrides "system|x,y,w,h". psx: glass isn't near-black (auto-detect fails), measured off a
# 10% grid. Handhelds (gb/gbc/gba): the screen rect is the bounding box of the Duimon *Glass* layer's
# non-transparent pixels (the screen-glass panel), normalized to the Device dims — measured by
# gen_handheld_shells.sh. The compositor fills this hole at the display aspect (SEMU_TAP_FILL), so the
# bbox can stay the full glass panel; the game centers in it at the LCD aspect.
OVERRIDES=(
  "psx|0.2500,0.1070,0.5000,0.6667"
  "gb|0.1843,0.2128,0.6314,0.2711"
  "gbc|0.1837,0.1910,0.6326,0.2794"
  "gba|0.2441,0.1134,0.5118,0.7610"
)

count=0; first=1
for row in "${TABLE[@]}"; do
  IFS='|' read -r sys rel aspect <<< "$row"
  aw="${aspect%:*}"; ah="${aspect#*:}"
  ovh=""; for o in "${OVERRIDES[@]}"; do case "$o" in "$sys|"*) ovh="${o#*|}";; esac; done
  if [ -n "$ovh" ]; then
    IFS=',' read -r hx hy hw hh <<< "$ovh"
    echo "OVERRIDE $sys: hole=$hx,$hy,$hw,$hh ($aspect)"
  else
    png="$(resolve_png "$sys" "$rel")"
    if [ -z "$png" ]; then echo "WARN: $sys: no PNG found under ASSET_ROOT for $rel — skipped"; continue; fi
    if ! read -r W H gx gy gw gh < <(measure_glass "$png"); then
      echo "WARN: $sys: glass auto-detect failed for $png — skipped (add a manual override)"; continue
    fi
    # snap: keep measured width + center, derive height from aspect (center-preserving) -> normalized
    read -r hx hy hw hh < <(awk -v W="$W" -v H="$H" -v gx="$gx" -v gy="$gy" -v gw="$gw" -v gh="$gh" -v aw="$aw" -v ah="$ah" 'BEGIN{
          cx=gx+gw/2.0; cy=gy+gh/2.0;
          holeW=gw; holeH=holeW*ah/aw;
          printf "%.4f %.4f %.4f %.4f\n", (cx-holeW/2.0)/W, (cy-holeH/2.0)/H, holeW/W, holeH/H }')
    cxn=$(awk -v gx="$gx" -v gw="$gw" -v W="$W" 'BEGIN{printf "%.4f",(gx+gw/2.0)/W}')
    echo "MEASURED $sys: img=${W}x${H} glass=(${gx},${gy},${gw},${gh}) centerX=$cxn  ->  hole=$hx,$hy,$hw,$hh ($aspect)"
  fi
  [ $first -eq 1 ] || echo "," >> "$tmpj"; first=0
  printf '  "%s": { "art": "%s", "aspect": "%s", "hole": [%s, %s, %s, %s] }' "$sys" "$rel" "$aspect" "$hx" "$hy" "$hw" "$hh" >> "$tmpj"
  count=$((count+1))
done

echo "" >> "$tmpj"; echo "}" >> "$tmpj"
mv "$tmpj" "$JSON"
echo "wrote $count bezel(s) -> $JSON"
