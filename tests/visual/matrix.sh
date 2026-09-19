#!/bin/sh
# Capture one system's visual matrix headlessly: default, alternate shader, alternate bezel,
# everything disabled. Each cell is a real launch; the four captures are tiled into one sheet.
# usage: matrix.sh OUT_DIR SYSTEM EMULATOR ROM SHADER_ALT BEZEL_ALT [extra launch args...]
# Env: SEMU_CAPTURE_WAIT (boot wait), SEMU_CAPTURE_DISPLAY (base display number), SEMU (cli).
set -eu
out="$1"; system="$2"; emulator="$3"; rom="$4"; shaderAlt="$5"; bezelAlt="$6"; shift 6
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$out"
base="${SEMU_CAPTURE_DISPLAY:-90}"
cell() {  # name settings-json
  SEMU_CAPTURE_DISPLAY="$base" "$here/capture.sh" "$out/$system-$1.png" "$emulator" --system "$system" --rom "$rom" --settings-json "$2" "$@" >/dev/null
}
cell default '{}'
cell shader-alt "{\"visual\":{\"systems\":{\"$system\":{\"shader_variant\":\"$shaderAlt\"}}}}"
cell bezel-alt "{\"visual\":{\"systems\":{\"$system\":{\"bezel_variant\":\"$bezelAlt\"}}}}"
cell disabled '{"visual":{"crt_shaders":false,"bezels":false}}'
montage -label '%t' -font DejaVu-Sans -pointsize 28 -tile 2x2 -geometry 960x540+8+8 -background '#202020' -fill white \
  "$out/$system-default.png" "$out/$system-shader-alt.png" "$out/$system-bezel-alt.png" "$out/$system-disabled.png" "$out/$system-sheet.png"
echo "$out/$system-sheet.png"
