#!/usr/bin/env bash
# gen-bezel.sh NATIVE_W NATIVE_H OUT_DIR
# Dynamic RetroArch bezel overlay. Detects the live screen resolution, computes the largest INTEGER
# render rect for the core's native res (matching RetroArch's video_scale_integer), and emits:
#   OUT_DIR/bezel.png  - a frame whose transparent window == that render rect (so the game shows
#                        through and the frame fills the margins on top)
#   OUT_DIR/bezel.cfg  - the RetroArch overlay descriptor referencing bezel.png
# No hardcoded sizes: everything derives from (screen res, native res). Recomputes per screen.
set -eu
NW="$1"; NH="$2"; OUT="$3"
mkdir -p "$OUT"
# Live screen res = the active DRM mode (gamescope presents at the panel res). Fallback 1280x800.
RES="$(cat /sys/class/drm/*/modes 2>/dev/null | grep -E '^[0-9]+x[0-9]+$' | head -1 || true)"
SW="${RES%x*}"; SH="${RES#*x}"
[ -n "${SW:-}" ] && [ "$SW" -gt 0 ] 2>/dev/null || SW=1280
[ -n "${SH:-}" ] && [ "$SH" -gt 0 ] 2>/dev/null || SH=800
# The Deck panel is physically portrait (800x1280) but gamescope presents landscape; the effective
# viewport RetroArch renders into is landscape. Force landscape (width = larger dimension).
if [ "$SW" -lt "$SH" ]; then t="$SW"; SW="$SH"; SH="$t"; fi
SX=$(( SW / NW )); SY=$(( SH / NH )); S=$(( SX < SY ? SX : SY )); [ "$S" -lt 1 ] && S=1
RW=$(( NW * S )); RH=$(( NH * S )); RX=$(( (SW - RW) / 2 )); RY=$(( (SH - RH) / 2 ))
python3 - "$SW" "$SH" "$RX" "$RY" "$RW" "$RH" "$OUT/bezel.png" <<'PY'
import sys, zlib, struct
SW,SH,RX,RY,RW,RH = map(int, sys.argv[1:7]); out = sys.argv[7]
DARK=(16,17,24,255); MID=(46,49,64,255); HI=(96,102,128,255); T=(0,0,0,0)
def col(d):
    if d<=2: return HI      # bright inner edge of the frame (definition)
    if d<=10: return MID    # bevel slope
    return DARK             # frame body
row=[]
raw=bytearray()
for y in range(SH):
    raw.append(0)  # PNG filter: none
    inrow = RY<=y<RY+RH
    for x in range(SW):
        if inrow and RX<=x<RX+RW:
            raw += bytes(T); continue
        dx = RX-x if x<RX else (x-(RX+RW-1) if x>=RX+RW else 0)
        dy = RY-y if y<RY else (y-(RY+RH-1) if y>=RY+RH else 0)
        raw += bytes(col(dx if dx>dy else dy))
def ch(t,d): c=t+d; return struct.pack('>I',len(d))+c+struct.pack('>I',zlib.crc32(c)&0xffffffff)
png=b'\x89PNG\r\n\x1a\n'+ch(b'IHDR',struct.pack('>IIBBBBB',SW,SH,8,6,0,0,0))+ch(b'IDAT',zlib.compress(bytes(raw),6))+ch(b'IEND',b'')
open(out,'wb').write(png)
PY
cat > "$OUT/bezel.cfg" <<O
overlays = 1
overlay0_overlay = bezel.png
overlay0_full_screen = true
overlay0_descs = 0
overlay0_rect = "0.0,0.0,1.0,1.0"
O
echo "bezel: screen ${SW}x${SH}, render ${RW}x${RH}+${RX}+${RY} (${NW}x${NH} x${S})"
