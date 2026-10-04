#!/usr/bin/env bash
# Renders Semu's Steam Input binding icons from Lucide with the recipe the committed icons were
# made with. config/input/steam/icons/PROVENANCE.md is the only table: its pinned commit and
# every row whose source is a backticked Lucide name. Rows drawn by hand are never rendered.
#
#   render-icons.sh OUTPUT_DIR [semu-name.png ...]   render every Lucide row, or the named ones
#   render-icons.sh --check                          re-render all into a scratch directory; each
#                                                     committed icon must match at RMSE 0
# Needs bash, curl, sed and ImageMagick 7 (magick). Nothing is ever deleted.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
icons="$here/../../config/input/steam/icons"
provenance="$icons/PROVENANCE.md"
commit="$(sed -n 's/.*`\([0-9a-f]\{40\}\)`.*/\1/p' "$provenance" | head -n 1)"
[ -n "$commit" ] || { echo "render-icons: no pinned Lucide commit in $provenance" >&2; exit 1; }

rows() {  # "semu-file.png lucide-name" for every Lucide-derived row
  sed -n 's/^| `\(semu-[a-z0-9-]*\.png\)` | `\([a-z0-9-]*\)` |$/\1 \2/p' "$provenance"
}

render() {  # $1 Semu file, $2 Lucide icon, $3 output directory
  local svg
  svg="$(curl -fsSL "https://raw.githubusercontent.com/lucide-icons/lucide/$commit/icons/$2.svg")"
  printf '%s\n' "$svg" | sed 's/currentColor/#FFFFFF/' \
    | magick -background none -density 384 - -filter Mitchell -resize 192x192 -gravity center -extent 256x256 -strip "PNG32:$3/$1"
}

if [ "${1:-}" = "--check" ]; then
  scratch="$(mktemp -d)"
  failed=0
  while read -r file source; do
    render "$file" "$source" "$scratch"
    metric="$(magick compare -metric RMSE "$scratch/$file" "$icons/$file" null: 2>&1 || true)"
    case "$metric" in
      "0 (0)") echo "same   $file ($source)" ;;
      *) echo "DIFFER $file ($source): RMSE $metric" >&2; failed=1 ;;
    esac
  done < <(rows)
  echo "rendered into $scratch"
  exit "$failed"
fi

output="${1:?usage: render-icons.sh OUTPUT_DIR [semu-name.png ...] | --check}"
shift
mkdir -p "$output"
wanted=" $* "
while read -r file source; do
  if [ $# -gt 0 ] && [ "${wanted#* $file }" = "$wanted" ]; then continue; fi
  render "$file" "$source" "$output"
  echo "$output/$file ($source)"
done < <(rows)
