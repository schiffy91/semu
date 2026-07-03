#!/usr/bin/env bash
# gen_handheld_shells.sh — build photoreal handheld device shells (+ color/model variants) from the
# Duimon Mega Bezel art.
#
# The Duimon handheld packs are multi-layer (Device = bare plastic body, Decal = printed logos/text,
# Glass = the screen-glass panel, Top = highlights). We composite Device+Decal into one flat shell PNG
# the tap compositor uses as bezel art, and derive each device's screen hole from the bounding box of the
# GLASS layer's non-transparent pixels (that panel == the device's screen), normalized to the Device dims.
# The compositor fills the hole at the display aspect (SEMU_TAP_FILL) so the game lands in the LCD.
#
# Variants (toggle-bezel a/b/c via the radial): gb/gbc get iconic colorways (recolor = desaturate then
# Multiply a solid color, preserving plastic shading + alpha); gba gets the GBA SP clamshell — a DIFFERENT
# model with its own glass, so its hole differs (printed below; wire into launcherTapScreenVariant).
#
# Emits: <OUT>/{gb,gb-b,gb-c,gbc,gbc-b,gbc-c,gba,gba-b}.png  (deploy under .../Mega_Bezel_Packs/semu-shells/)
# Prints the measured hole line for each device (x,y,w,h normalized) for the manifest OVERRIDES /
# launcherTapScreenVariant.
#
# Usage: gen_handheld_shells.sh <DUIMON_GRAPHICS_DIR> [OUT_DIR]
set -euo pipefail
IM="$(command -v magick || command -v convert)"
[ -n "$IM" ] || { echo "ERROR: ImageMagick (magick/convert) not found"; exit 1; }
GFX="${1:?usage: gen_handheld_shells.sh <Duimon Graphics dir> [out]}"
REPO="$(cd "$(dirname "$0")/../../../../.." && pwd)"
OUT="${2:-$REPO/src/generated/assets/semu-shells}"
mkdir -p "$OUT"

# measure screen hole = bbox of the Glass layer's non-transparent pixels, normalized to the Device dims.
# here-strings (<<<) — `read` from a no-trailing-newline pipe returns non-zero and trips `set -e`.
emit_hole() {  # $1=device png $2=glass png $3=label
  read -r W H <<< "$("$IM" "$1" -format "%w %h" info:)"
  read -r gw gh gx gy <<< "$("$IM" "$2" -trim -format "%w %h %X %Y" info: | tr -d '+')"
  awk -v W="$W" -v H="$H" -v gw="$gw" -v gh="$gh" -v gx="$gx" -v gy="$gy" -v s="$3" \
    'BEGIN{ printf "  %-7s hole = %.4f,%.4f,%.4f,%.4f  (glass %sx%s+%s+%s in %sx%s)\n", s, gx/W, gy/H, gw/W, gh/H, gw,gh,gx,gy,W,H }'
}
flat() {  # $1=out  $2..=layers (bottom->top) — flatten the device LAYER STACK, downscale (hole is normalized).
  # IMPORTANT: -flatten (not pairwise -composite) so ALL layers merge. The stack must include each device's
  # button/detail layers: Top adds the molded button definition (d-pad/A-B); GBC's actual d-pad+A/B buttons
  # live on a SEPARATE Device_LED layer (black buttons kept off the recolorable body). Glass (screen) and the
  # standalone LED layer are EXCLUDED. Omitting Top/Device_LED is why shells looked button-less.
  local out="$1"; shift
  "$IM" "$@" -background none -flatten -resize '2048x2048>' "$out"
}
colorway() {  # $1=base shell $2=out $3=#color — desaturate then Multiply a solid color (keeps shading+alpha)
  "$IM" "$1" -modulate 100,0,100 \( +clone -fill "$3" -colorize 100% \) -compose Multiply -composite -alpha on "PNG32:$2"
}

GB="$GFX/Nintendo_Game_Boy"; GBC="$GFX/Nintendo_Game_Boy_Color"; GBA="$GFX/Nintendo_GBA"; SP="$GFX/Nintendo_GBA_SP"
# --- base shells (full layer stack) + holes ---
flat "$OUT/gb.png"     "$GB/Gameboy_DMG01_Device.png" "$GB/Gameboy_DMG01_Decal.png" "$GB/Gameboy_DMG01_Top.png"
flat "$OUT/gbc.png"    "$GBC/GBC_Device.png" "$GBC/GBC_Device_LED.png" "$GBC/GBC_Decal.png" "$GBC/GBC_Top.png"
flat "$OUT/gba.png"    "$GBA/GBA_Device.png" "$GBA/GBA_Decal.png" "$GBA/GBA_Top.png"
flat "$OUT/gba-b.png"  "$SP/GBA_SP_Device.png" "$SP/GBA_SP_Decal.png" "$SP/GBA_SP_Top.png"   # gba variant b = SP clamshell
emit_hole "$GB/Gameboy_DMG01_Device.png" "$GB/Gameboy_DMG01_Glass.png" gb
emit_hole "$GBC/GBC_Device.png"          "$GBC/GBC_Glass.png"          gbc
emit_hole "$GBA/GBA_Device.png"          "$GBA/GBA_Glass.png"          gba
emit_hole "$SP/GBA_SP_Device.png"        "$SP/GBA_SP_Glass.png"        gba-b
# --- screen-glass layers (live cutout MASK in .a + reflections in .rgb) ---
glass(){ "$IM" "$1" -resize '2048x2048>' "PNG32:$2"; }   # Glass layer, downscaled, RGBA preserved
glass "$GB/Gameboy_DMG01_Glass.png" "$OUT/gb-glass.png"
glass "$GBC/GBC_Glass.png"          "$OUT/gbc-glass.png"
glass "$GBA/GBA_Glass.png"          "$OUT/gba-glass.png"
glass "$SP/GBA_SP_Glass.png"        "$OUT/gba-b-glass.png"   # gba variant b = SP clamshell screen
# --- colorways (same model -> share the base hole) ---
colorway "$OUT/gb.png"  "$OUT/gb-b.png"  '#d23440'   # Play It Loud red
colorway "$OUT/gb.png"  "$OUT/gb-c.png"  '#2f9e4f'   # Play It Loud green
colorway "$OUT/gbc.png" "$OUT/gbc-b.png" '#c0294b'   # Berry
colorway "$OUT/gbc.png" "$OUT/gbc-c.png" '#7a3fb0'   # Grape
echo "shells + variants -> $OUT"
ls "$OUT"
