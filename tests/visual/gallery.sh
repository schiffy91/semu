#!/usr/bin/env bash
# The bezel gallery from the real renderer on this Mac (M11.5 and M11.6): every system's variants at
# Deck (1280x800) and 4K (3840x2160), each fixed-layout screen verified by lighting a flat card and
# measuring where it lands with the tube's bend and light off (within 2 px of the package's picture
# rectangle), the dimensions table and the plate overlays, written as OUT_DIR/index.html. The flat card is
# measured in each of the four Fit states (GALLERY_STATES narrows them): the Fit state's sizing is the
# renderer's own, read from its debug line's bezelrect (where it placed the package's canvas), and what this
# checks is the package geometry on that canvas, so the picture lands on the package's picture rectangle in
# every state. Scratch renders and logs stay in a mktemp directory, never removed.
# usage: gallery.sh [--quick] OUT_DIR [system...]
# --quick renders the Deck size only and skips the flat-card verification (the fast loop).
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
quick=0; [ "${1:-}" = "--quick" ] && { quick=1; shift; }
out="$1"; shift
sizes="1280x800 3840x2160"; [ "$quick" = 1 ] && sizes="1280x800"
states="${GALLERY_STATES:-game bezel game_fractional bezel_fractional}"  # the four Fit states each screen is measured in
make -C "$root" --no-print-directory build/semu build/render-host >/dev/null
nix build --no-warn-dirty --out-link "$root/build/asset-root" "$root#asset-root"
assets="$root/build/asset-root"
semu="$root/build/semu"
host="$root/build/render-host"
systems=("$@")
[ ${#systems[@]} -gt 0 ] || mapfile -t systems < <(cd "$root/config/systems" && for s in *; do jq -e '.variants | length > 0' "$s/bezels.json" >/dev/null 2>&1 && echo "$s"; done)
mkdir -p "$out/cells" "$out/overlays"
work="$(mktemp -d)"  # the renders and their logs, left behind, never removed
failures=0
verdicts=""

environment() {  # SYSTEM VARIANT [SHADER [FIT_STATE]]: the launcher's render environment for that choice (the system's own Fit state without one)
  "$semu" render-env --system "$1" --project "$root/config" --asset-root "$assets" \
    --settings-json "{\"visual\":{\"systems\":{\"$1\":{\"bezel_variant\":\"$2\"${3:+,\"shader_variant\":\"$3\"}${4:+,\"placement\":\"$4\"}}}}}"
}

placement_only() {  # ENV-LINE: a screen's look without curvature, vignette, bloom or reflection, its shape, radius, fit and inset kept
  case "$1" in
    SEMU_RENDER_SCREEN_*_LOOK*=*) IFS=, read -r shape radius exponent fit inset _ <<< "${1#*=}"; echo "${1%%=*}=$shape,$radius,$exponent,$fit,$inset,0,0,0,0,0" ;;
    SEMU_RENDER_SCREEN_*_REFLECT*=*) echo "${1%%=*}=0,0,0" ;;
    *) echo "$1" ;;
  esac
}

render() {  # SYSTEM VARIANT WIDTH HEIGHT OUT.ppm [CARD LIT FIT_STATE]: a lit card measures placement, so its shader and the tube's bend and light are off; its log (the renderer's debug lines) is $work/last.log
  ( while IFS= read -r line; do [ -n "${6:-}" ] && line="$(placement_only "$line")"; export "$line"; done < <(environment "$1" "$2" "${6:+none}" "${8:-}")
    SEMU_RENDER_DEBUG=1 RENDER_HOST_CARD="${6:-}" RENDER_HOST_LIT="${7:-}" RENDER_HOST_ASPECT="$wide" "$host" "$3" "$4" "$5" ) 2>"$work/last.log"
  local status=$?
  cat "$work/last.log" >> "$work/render.log"
  return "$status"
}

expected() {  # PACKAGE SCREEN HEIGHT CANVAS ASPECT [NATIVE]: the game fitted at ASPECT inside the package's picture rectangle (else the opening less its inset) on CANVAS, the rectangle the renderer placed the package's canvas on ("X,Y WxH" from its debug line's bezelrect, rows from the bottom), in output pixels, as W H X Y; the Fit state's sizing is the renderer's own, the package geometry is what this checks. NATIVE (WxH): a DS or 3DS shell's screen in an integer state, drawn at the largest whole step its fitted rectangle holds (1% grace, 1x from half of it, RendererDualShell.secondStep) centred on it
  jq -r --argjson screen "$2" '(.screens[$screen] | .image // (.tube as $tube | ((.inset // 0) * 0.45 * ([$tube.w, $tube.h] | min)) as $inset | {x: ($tube.x + $inset), y: ($tube.y + $inset), w: ($tube.w - 2 * $inset), h: ($tube.h - 2 * $inset)})) as $picture | [.canvas.w, .canvas.h, $picture.x, $picture.y, $picture.w, $picture.h] | @tsv' \
    "$root/config/bezels/$1/bezel.json" | awk -v height="$3" -v canvas="$4" -v aspect="$5" -v whole="${6:-}" -F'\t' '{
      split(canvas, rect, /[ ,x]/); left = rect[1]; bottom = rect[2]; placedWidth = rect[3]; placedHeight = rect[4]
      scale = placedWidth / $1; top = height - bottom - placedHeight
      pictureWidth = $5 * scale; pictureHeight = $6 * scale; pictureX = left + $3 * scale; pictureY = top + $4 * scale
      if (aspect > 0 && pictureWidth / pictureHeight > aspect) { fitted = pictureHeight * aspect; pictureX += (pictureWidth - fitted) / 2; pictureWidth = fitted }
      else if (aspect > 0) { fitted = pictureWidth / aspect; pictureY += (pictureHeight - fitted) / 2; pictureHeight = fitted }
      if (whole != "") {
        split(whole, native, "x"); across = pictureWidth / native[1]; down = pictureHeight / native[2]; held = across < down ? across : down
        step = int(held * 1.01); if (step < 1) step = held >= 0.5 ? 1 : 0
        if (step >= 1) { centreX = pictureX + pictureWidth / 2; centreY = pictureY + pictureHeight / 2; pictureWidth = step * native[1]; pictureHeight = step * native[2]; pictureX = centreX - pictureWidth / 2; pictureY = centreY - pictureHeight / 2 }
      }
      printf "%d %d %d %d\n", pictureWidth + 0.5, pictureHeight + 0.5, pictureX + 0.5, pictureY + 0.5 }'
}

measure() {  # WHITE.ppm BLACK.ppm: where the lit picture landed, as W H X Y; a 1-px black border keeps a picture that covers the whole screen measurable (-trim of a uniform image finds nothing)
  magick "$1" "$2" -compose difference -composite -colorspace gray -threshold 50% -bordercolor black -border 1 -trim -format '%w %h %X %Y\n' info: | tr -d '+' | awk '{ print $1, $2, $3 - 1, $4 - 1 }'
}

for system in "${systems[@]}"; do
  for variant in $(jq -r '.variants[].id' "$root/config/systems/$system/bezels.json"); do
    package="$(jq -r --arg id "$variant" '.variants[] | select(.id == $id) | .bezel' "$root/config/systems/$system/bezels.json")"
    wide=""; [ "$(jq -r --arg id "$variant" '.variants[] | select(.id == $id) | .output // ""' "$root/config/systems/$system/bezels.json")" = widescreen ] && wide="1.777778"  # a variant declaring the widescreen output shows a 16:9 title
    chosen="$(jq -r --arg id "$variant" '.variants[] | select((.outputs // {}) | to_entries | any(.value == $id)) | .id' "$root/config/systems/$system/bezels.json" | head -1)"
    [ -n "$chosen" ] || chosen="$variant"  # render-env never selects the Wii's tv_wide by name: a launch reaches it through tv on a 16:9 picture, the renderer's _B keys
    for size in $sizes; do
      width="${size%x*}"; height="${size#*x}"
      render "$system" "$chosen" "$width" "$height" "$work/card.ppm" || { echo "$system/$variant $size: render failed"; failures=$((failures + 1)); continue; }
      magick "$work/card.ppm" -resize '1280x>' "PNG24:$out/cells/$system-$variant-$size.png"
      layout="$(jq -r '.layout // "fixed"' "$root/config/bezels/$package/bezel.json")"
      [ "$layout" = "fixed" ] && [ "$quick" = 0 ] || continue  # computed layouts carry no picture rectangle to verify
      count="$(jq '.screens | length' "$root/config/bezels/$package/bezel.json")"
      for state in $states; do
      for (( screen = 0; screen < count; screen++ )); do
        jq -e --argjson screen "$screen" '(.screens[$screen] | .image // .tube).w > 0' "$root/config/bezels/$package/bezel.json" >/dev/null 2>&1 || continue
        render "$system" "$chosen" "$width" "$height" "$work/white.ppm" white "$screen" "$state"
        canvas="$(grep -a -o 'bezelrect [0-9.-]*,[0-9.-]* [0-9.]*x[0-9.]*' "$work/last.log" | head -1 | cut -d' ' -f2-)"  # where the renderer placed the canvas in this Fit state
        render "$system" "$chosen" "$width" "$height" "$work/black.ppm" black "$screen" "$state"
        read -r measuredWidth measuredHeight measuredX measuredY < <(measure "$work/white.ppm" "$work/black.ppm")
        aspect="$wide"
        if [ -z "$aspect" ]; then
          if [ "$count" = 1 ]; then aspect="$(environment "$system" "$chosen" | sed -n 's/^SEMU_RENDER_ASPECT=//p' | awk -F: '{ print $1 / $2 }')"
          else aspect="$(environment "$system" "$chosen" | sed -n "s/^SEMU_RENDER_SURFACE_${screen}_NATIVE=//p" | awk -Fx '{ print $1 / $2 }')"; fi
          crop="$(environment "$system" "$chosen" | sed -n "s/^SEMU_RENDER_SURFACE_${screen}_CROP=//p")"  # a cropped picture keeps its pixels square (the N64's 313x237 of 320x240), as RendererSurfaceCropping draws it
          native="$(environment "$system" "$chosen" | sed -n "s/^SEMU_RENDER_SURFACE_${screen}_NATIVE=//p")"
          [ -z "$crop" ] || aspect="$(awk -v aspect="$aspect" -v crop="$crop" -v native="$native" 'BEGIN { split(crop, cut, ","); split(native, size, "x"); printf "%.6f", aspect * ((size[1] - cut[1] - cut[3]) / size[1]) / ((size[2] - cut[2] - cut[4]) / size[2]) }')"
        fi
        whole=""; [ "$count" = 2 ] && case "$state" in game|bezel) whole="$native" ;; esac  # a DS or 3DS shell's integer states: each screen at its own whole step
        read -r wantWidth wantHeight wantX wantY < <(expected "$package" "$screen" "$height" "$canvas" "$aspect" "$whole")
        worst=0
        for delta in $((measuredX - wantX)) $((measuredY - wantY)) $((measuredX + measuredWidth - wantX - wantWidth)) $((measuredY + measuredHeight - wantY - wantHeight)); do
          delta="${delta#-}"; [ "$delta" -gt "$worst" ] && worst="$delta"
        done
        verdict="pass"; [ "$worst" -le 2 ] || { verdict="FAIL"; failures=$((failures + 1)); }
        echo "$system/$variant $size $state screen $screen: worst edge $worst px ($verdict)"
        verdicts+="<tr class=\"$verdict\"><td>$system</td><td>$variant</td><td>$size</td><td>$state</td><td>$screen</td><td>${measuredWidth}x${measuredHeight}+${measuredX}+${measuredY}</td><td>${wantWidth}x${wantHeight}+${wantX}+${wantY}</td><td>$worst</td><td>$verdict</td></tr>"
      done
      done
    done
  done
done

dimensions="$(for file in "$root"/config/bezels/*/bezel.json; do jq -r '. as $package | select(.layout == "fixed" or .layout == null) | .screens | to_entries[] | select(.value.image != null) | "<tr><td>\($package.id)</td><td>\(.value.id)</td><td>\($package.canvas.w)x\($package.canvas.h)</td><td>\(.value.tube | "\(.w)x\(.h)+\(.x)+\(.y)")</td><td>\(.value.image | "\(.w)x\(.h)+\(.x)+\(.y)")</td><td>\((.value.image.w / .value.image.h * 1000 | round) / 1000)</td><td>\($package.upstream.preset // "semu plate")</td></tr>"' "$file"; done)"
overlays=""
for file in "$root"/config/bezels/*/bezel.json; do
  [ "$quick" = 0 ] || break  # overlays resize every plate: the full run only
  package="$(jq -r .id "$file")"; layer="$(jq -r '.canvas_layer // empty' "$file")"
  [ -n "$layer" ] || continue
  plate="$(ls "$assets/share/semu/assets/bezels/layers/$package/$layer".* 2>/dev/null | head -1 || true)"
  [ -n "$plate" ] || plate="$assets/share/semu/$(jq -r '.art // empty' "$file")"
  [ -f "$plate" ] || continue
  draws="$(jq -r '.screens[] | (if .tube then "-stroke \u0027#3ddc84\u0027 -draw \"rectangle \(.tube.x),\(.tube.y) \(.tube.x + .tube.w),\(.tube.y + .tube.h)\"" else empty end), (if .image then "-stroke \u0027#ffb454\u0027 -draw \"rectangle \(.image.x),\(.image.y) \(.image.x + .image.w),\(.image.y + .image.h)\"" else empty end)' "$file" | tr '\n' ' ')"
  eval magick "\"$plate\"" -resize "$(jq -r '"\(.canvas.w)x\(.canvas.h)!"' "$file")" -fill none -strokewidth 8 $draws -resize 640x "PNG24:$out/overlays/$package.png"
  overlays+="<figure><img loading=\"lazy\" src=\"overlays/$package.png\"><figcaption>$package · opening green, picture amber</figcaption></figure>"
done

{
  cat <<'HEAD'
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Semu Bezel Gallery</title>
<style>:root{--bg:#16171a;--panel:#1f2126;--line:#2c2f36;--text:#e8e8e8;--muted:#9aa0aa;--accent:#ffb454}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.45 system-ui,sans-serif}main{max-width:1400px;margin:0 auto;padding:16px}h2{margin:28px 0 8px}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:10px}figure{margin:0;background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:6px}img{width:100%;display:block;border-radius:4px}figcaption{color:var(--muted);font-size:12px;margin-top:4px}table{border-collapse:collapse;width:100%;font-size:12px}td,th{border-bottom:1px solid var(--line);padding:4px 6px;text-align:left}tr.FAIL td{color:#ff6b6b}</style></head><body><main><h1>Semu bezel gallery</h1>
HEAD
  echo "<p>Rendered by the real renderer (tests/visual/render_host.btrc) from the bundle's data, $(date -u +%Y-%m-%dT%H:%MZ), revision $(git -C "$root" rev-parse --short HEAD). Verification failures: $failures.</p>"
  echo "<h2>Cells</h2><div class=\"grid\">"
  for cell in "$out"/cells/*.png; do name="$(basename "$cell" .png)"; echo "<figure><img loading=\"lazy\" src=\"cells/$name.png\"><figcaption>$name</figcaption></figure>"; done
  echo "</div><h2>Verification (flat card, within 2 px)</h2><table><tr><th>system</th><th>variant</th><th>size</th><th>fit</th><th>screen</th><th>measured</th><th>package</th><th>worst px</th><th></th></tr>$verdicts</table>"
  echo "<h2>Dimensions</h2><table><tr><th>package</th><th>screen</th><th>canvas</th><th>opening</th><th>picture</th><th>aspect</th><th>preset</th></tr>$dimensions</table>"
  echo "<h2>Overlays</h2><div class=\"grid\">$overlays</div></main></body></html>"
} > "$out/index.html"
echo "gallery: $out/index.html ($failures verification failures)"
exit "$failures"
