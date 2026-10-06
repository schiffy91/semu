#!/bin/bash
# Is Semu's touch crosshair drawn in a capture, centred on a point? The crosshair is
# renderer_cursor.btrc's (a contract keeps the rows below the same), drawn at one whole step per
# 400 rows of the capture (2 on the Deck's 800: 30x30, 64 white arm and 224 black outline pixels),
# its centre cell's middle pixel on the pointer. Counts the arm pixels that are white and the
# outline pixels that are black there (within 3 percent), so a white or black background alone
# never passes as the crosshair.
#
#   cursor-crosshair.sh PNG X Y   # prints "white W/64 outline O/224 at X,Y"; exit 0 the whole crosshair
#                                 # is there, 1 it is not, 2 unchecked (no ImageMagick, or the crosshair
#                                 # would cross the capture's edge)
# Reads only PNG and writes nothing: each mask reaches ImageMagick on file descriptor 3.
set -u
rows=(
  "      BBB      " "      BWB      " "      BWB      " "      BWB      " "      BWB      " "      BBB      "
  "BBBBBB   BBBBBB" "BWWWWB   BWWWWB" "BBBBBB   BBBBBB"
  "      BBB      " "      BWB      " "      BWB      " "      BWB      " "      BWB      " "      BBB      "
)
hotspot=7  # the centre cell, across and down
png="${1:?usage: cursor-crosshair.sh PNG X Y}"; point_x="${2:?usage: cursor-crosshair.sh PNG X Y}"; point_y="${3:?usage: cursor-crosshair.sh PNG X Y}"
case "$point_x$point_y" in ''|*[!0-9]*) echo "unchecked: X and Y must be whole pixels"; exit 2 ;; esac
magick=""
if command -v magick > /dev/null 2>&1; then magick=magick; elif command -v convert > /dev/null 2>&1; then magick=convert; fi
[ -n "$magick" ] || { echo "unchecked: no ImageMagick here (run cursor-crosshair.sh on the fetched capture)"; exit 2; }
[ -s "$png" ] || { echo "unchecked: no capture $png"; exit 2; }
size="$("$magick" "$png" -format '%w %h' info: 2> /dev/null)"
read -r width height <<< "$size"
[ -n "${height:-}" ] || { echo "unchecked: $png is not an image"; exit 2; }
scale=$((height / 400)); [ "$scale" -ge 1 ] || scale=1; [ "$scale" -le 8 ] || scale=8
box_width=$((15 * scale)); box_height=$((15 * scale))
offset=$((hotspot * scale + scale / 2))  # renderer_cursor.btrc's hotspot(): the centre cell's middle pixel
left=$((point_x - offset)); top=$((point_y - offset))
[ "$left" -ge 0 ] && [ "$top" -ge 0 ] && [ $((left + box_width)) -le "$width" ] && [ $((top + box_height)) -le "$height" ] \
  || { echo "unchecked: the crosshair at $point_x,$point_y would cross the ${width}x$height capture's edge"; exit 2; }

mask() {  # CELL: a PGM at the capture's scale, 1 where the crosshair has that cell (W arm, B outline)
  local line column repeat across pixels
  echo "P2 $box_width $box_height 1"
  for line in "${rows[@]}"; do
    pixels=""
    for ((column = 0; column < 15; column++)); do
      for ((across = 0; across < scale; across++)); do [ "${line:column:1}" = "$1" ] && pixels="$pixels 1" || pixels="$pixels 0"; done
    done
    for ((repeat = 0; repeat < scale; repeat++)); do echo "$pixels"; done
  done
}
cells() { local line count=0 rest; for line in "${rows[@]}"; do rest="${line//[!$1]/}"; count=$((count + ${#rest})); done; echo $((count * scale * scale)); }
matches() {  # COLOUR CELL: the CELL pixels of the box that are COLOUR
  local other=black; [ "$1" = black ] && other=white
  "$magick" "$png" -crop "${box_width}x$box_height+$left+$top" +repage -alpha off -fuzz 3% -fill "$other" +opaque "$1" -fill "$1" -opaque "$1" \
    $([ "$1" = black ] && echo -negate) -colorspace gray "pgm:fd:3" -compose multiply -composite -format '%[fx:round(mean*w*h)]' info: 3< <(mask "$2")
}
white="$(matches white W)"; outline="$(matches black B)"; arm_total="$(cells W)"; outline_total="$(cells B)"
echo "white $white/$arm_total outline $outline/$outline_total at $point_x,$point_y"
[ "$white" = "$arm_total" ] && [ "$outline" = "$outline_total" ]
