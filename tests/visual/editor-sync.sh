#!/usr/bin/env bash
# Does the bezel editor show what the renderer draws? For every bezel variant (or the given system[:variant] cells):
# production is build/render-host fed by `semu render-env` with the shader off, the editor is `semu bezel edit` in its
# capture mode (?capture=WxH, headless Chrome), both drawing the same card. Each pair becomes
# OUT_DIR/<system>-<variant>-{editor,production,diff,side}.png, a row of OUT_DIR/metrics.tsv and OUT_DIR/index.html.
# usage: editor-sync.sh [--card white|black|test] [--placement game|bezel|game_fractional|bezel_fractional] [--size WxH] OUT_DIR [system[:variant]...]
# --card test draws the editor's test card on both sides (RENDER_HOST_CARD=editor). --placement overrides the system's.
# Exits with the number of failing cells: a render or capture that failed, framing more than 1 px apart, or any pixel
# whose largest channel differs by more than 10 % (the editor must show what the renderer draws).
# Metric: the share of pixels whose largest channel differs by more than 10 % (and 3 %) once both sides are blurred by 1 px
# (so nearest-neighbour aliasing is not a difference), the mean absolute error of the raw images, and the bounding box
# of the largest connected differing region; the canvas share counts only the canvas on screen (the renderer paints its
# background plate around a letterboxed canvas, the editor paints black). Framing: production's canvas rectangle comes
# from its debug line and the editor's from its console (it frames the canvas itself, integer placements included, for
# the system and variant named in ?preview=); the framing column says whether they agree within 1 px. A computed DS/3DS
# layout is the whole screen on both sides.
# A variant that declares an output (the Wii's 16:9 TV, "output": "widescreen") is not in the radial's bezel list, so
# `semu render-env` never selects it by name: the owner reaches it through the variant that lists it under "outputs" on
# that output's picture, which the renderer answers with the _B keys. Production renders it the same way (the base
# variant, a 16:9 card); the editor previews the variant itself. A cell whose production debug line names another
# package than the variant's own fails (PACKAGE DIFFERS): it would compare the editor against a state no launch reaches.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
card=white; placement=""; size=1280x800
while [ $# -gt 0 ]; do
  case "$1" in
    --card) card="$2"; shift 2 ;;
    --placement) placement="$2"; shift 2 ;;
    --size) size="$2"; shift 2 ;;
    *) break ;;
  esac
done
[ $# -ge 1 ] || { echo "usage: $0 [--card white|black|test] [--placement game|bezel|game_fractional|bezel_fractional] [--size WxH] OUT_DIR [system[:variant]...]" >&2; exit 64; }
case "$card" in white|black|test) ;; *) echo "editor-sync: --card is white, black or test" >&2; exit 64 ;; esac
out="$1"; shift
width="${size%x*}"; height="${size#*x}"
chrome="${CHROME:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}"
[ -x "$chrome" ] || { echo "editor-sync: no Chrome at $chrome (set CHROME)" >&2; exit 1; }
make -C "$root" --no-print-directory build/semu build/render-host bezel-tree >/dev/null
nix build --no-warn-dirty --out-link "$root/build/asset-root" "$root#asset-root"
semu="$root/build/semu"; host="$root/build/render-host"; assets="$root/build/asset-root"
mkdir -p "$out"
work="$(mktemp -d)"  # scratch: render logs, Chrome profiles, masks; left in place
cells=("$@")
if [ ${#cells[@]} -eq 0 ]; then
  mapfile -t cells < <(cd "$root/config/systems" && for system in *; do jq -e '.variants | length > 0' "$system/bezels.json" >/dev/null 2>&1 && echo "$system"; done)
fi

port=""
for candidate in $(seq 28765 28965); do nc -z 127.0.0.1 "$candidate" 2>/dev/null || { port="$candidate"; break; }; done
[ -n "$port" ] || { echo "editor-sync: no free loopback port" >&2; exit 1; }
"$semu" bezel edit --project "$root/config" --port "$port" >"$work/editor-server.log" 2>&1 &
server=$!
stop_server() {  # by PID: TERM, then verify it is gone
  kill -TERM "$server" 2>/dev/null || true
  for _ in $(seq 1 50); do kill -0 "$server" 2>/dev/null || break; sleep 0.1; done
  if kill -0 "$server" 2>/dev/null; then echo "editor-sync: editor server $server is still running" >&2; else echo "editor server (pid $server) stopped"; fi
}
trap stop_server EXIT
for _ in $(seq 1 300); do curl -sf -o /dev/null "http://127.0.0.1:$port/api/packages" && break; kill -0 "$server" 2>/dev/null || { cat "$work/editor-server.log" >&2; exit 1; }; sleep 0.1; done
curl -sf -o /dev/null "http://127.0.0.1:$port/api/packages" || { echo "editor-sync: the editor server never answered" >&2; exit 1; }
curl -sf "http://127.0.0.1:$port/api/packages" > "$work/packages.json"
echo "editor on http://127.0.0.1:$port/ (pid $server), scratch $work"

environment() {  # SYSTEM VARIANT: the launcher's render environment with the shader off
  "$semu" render-env --system "$1" --project "$root/config" --asset-root "$assets" \
    --settings-json "{\"visual\":{\"systems\":{\"$1\":{\"bezel_variant\":\"$2\",\"shader_variant\":\"none\"${placement:+,\"placement\":\"$placement\"}}}}}"
}

production() {  # SYSTEM VARIANT OUT.png WIDE LOG: the real renderer, its debug line kept in LOG for the package and the canvas rectangle
  local hostCard=""; [ "$card" = test ] && hostCard=editor || hostCard="$card"
  ( while IFS= read -r line; do export "$line"; done < <(environment "$1" "$2")
    SEMU_RENDER_DEBUG=1 RENDER_HOST_CARD="$hostCard" RENDER_HOST_ASPECT="$4" "$host" "$width" "$height" "$work/production.ppm" ) 2>"$5" \
    && magick "$work/production.ppm" "PNG24:$3"
}

chosen_variant() {  # BEZELS VARIANT: the bezel choice that reaches VARIANT, itself unless it declares an output; empty when no variant lists it under "outputs"
  if [ -z "$(jq -r --arg id "$2" '.variants[] | select(.id == $id) | .output // ""' "$1")" ]; then echo "$2"; return; fi
  jq -r --arg id "$2" '.variants[] | select((.outputs // {}) | to_entries | any(.value == $id)) | .id' "$1" | head -1
}

drawn_package() {  # LOG: the package production's debug line names
  sed -n 's/^semu-renderer: frame [0-9]* bezel \([^ ]*\) fb .*/\1/p' "$1" | head -1
}

rendered_rect() {  # LOG: production's canvas rectangle as X Y W H from the top left, empty when it drew none
  sed -n 's/.* bezelrect \(-\{0,1\}[0-9]*\),\(-\{0,1\}[0-9]*\) \([0-9]*\)x\([0-9]*\) .*/\1 \2 \3 \4/p' "$1" | head -1 \
    | awk -v height="$height" '$3 > 0 && $4 > 0 { printf "%d %d %d %d\n", $1, height - $2 - $4, $3, $4 }'
}

capture() {  # URL OUT.png: headless Chrome on its own profile, stopped by PID once the screenshot is written; the page's console reaches $profile.log
  local profile log pid
  profile="$(mktemp -d "$work/chrome.XXXXXX")"; log="$profile.log"; captureLog="$log"
  "$chrome" --headless=new --user-data-dir="$profile" --no-first-run --no-default-browser-check --use-mock-keychain --password-store=basic \
    --disable-extensions --disable-background-networking --disable-component-update --disable-sync --hide-scrollbars --force-device-scale-factor=1 \
    --enable-logging=stderr --log-level=0 \
    --window-size="$width,$height" --virtual-time-budget=15000 --screenshot="$2" "$1" >"$log" 2>&1 &
  pid=$!
  for _ in $(seq 1 900); do grep -q "bytes written to file" "$log" && break; kill -0 "$pid" 2>/dev/null || break; sleep 0.1; done
  kill -TERM "$pid" 2>/dev/null || true
  for _ in $(seq 1 50); do kill -0 "$pid" 2>/dev/null || break; sleep 0.1; done
  if kill -0 "$pid" 2>/dev/null; then echo "editor-sync: Chrome $pid did not stop" >&2; fi
  grep -q "bytes written to file" "$log" && [ -s "$2" ]
}

compare() {  # EDITOR.png PRODUCTION.png DIFF.png "X Y W H": prints SHARE10 SHARE3 CANVAS_SHARE10 MAE REGION AREA (REGION = WxH+X+Y of the largest region over 10 %)
  magick "$1" -alpha off -blur 0x1 "$work/editor-soft.miff"
  magick "$2" -alpha off -blur 0x1 "$work/production-soft.miff"
  magick "$work/editor-soft.miff" "$work/production-soft.miff" -compose difference -composite -separate -evaluate-sequence max "$work/delta.miff"  # the largest channel difference per pixel
  magick "$work/delta.miff" -threshold 10% "$work/mask.png"
  local share subtle mae region box
  share="$(magick "$work/mask.png" -format '%[fx:mean*100]' info:)"
  subtle="$(magick "$work/delta.miff" -threshold 3% -format '%[fx:mean*100]' info:)"
  local canvasLeft canvasTop canvasWidth canvasHeight inside
  read -r canvasLeft canvasTop canvasWidth canvasHeight <<<"$4"
  read -r canvasLeft canvasTop canvasWidth canvasHeight < <(awk -v left="$canvasLeft" -v top="$canvasTop" -v across="$canvasWidth" -v down="$canvasHeight" -v width="$width" -v height="$height" \
    'BEGIN { right = left + across > width ? width : left + across; bottom = top + down > height ? height : top + down; left = left < 0 ? 0 : left; top = top < 0 ? 0 : top; print left, top, right - left, bottom - top }')
  inside="$(magick "$work/mask.png" -crop "${canvasWidth}x${canvasHeight}+${canvasLeft}+${canvasTop}" +repage -format '%[fx:mean*100]' info:)"  # the canvas on screen only: the renderer's background plate outside it is not the package
  mae="$( { magick compare -metric MAE "$1" "$2" null: 2>&1 || true; } | sed -n 's/.*(\([0-9.e+-]*\)).*/\1/p')"  # compare exits 1 whenever the images differ
  region="$(magick "$work/mask.png" -morphology Close Disk:2 -define connected-components:verbose=true -define connected-components:exclude-header=true \
    -connected-components 8 null: 2>/dev/null | awk '$NF ~ /255|white/ && $4 + 0 > best { best = $4 + 0; box = $2 } END { if (best > 0) print box, best; else print "none 0" }')"
  box="${region% *}"
  local draw=()
  if [ "$box" != none ]; then
    IFS='x+' read -r boxWidth boxHeight boxX boxY <<<"$box"
    draw=(-fill none -stroke '#ff3b30' -strokewidth 2 -draw "rectangle $boxX,$boxY $((boxX + boxWidth - 1)),$((boxY + boxHeight - 1))")
  fi
  magick "$1" "$2" -alpha off -compose difference -composite -level 0,33% "${draw[@]}" "PNG24:$3"
  printf '%.3f %.3f %.3f %.5f %s\n' "$share" "$subtle" "$inside" "${mae:-0}" "$region"
}

panel() {  # IN.png LABEL OUT.miff: a half-size panel with its caption
  magick "$1" -resize 50% -background '#16171a' -gravity north -splice 0x26 -font DejaVu-Sans -pointsize 15 -fill '#e8e8e8' -annotate +0+5 "$2" "$3" 2>>"$work/magick.log"  # fontconfig warns without a config file
}

rows=""
printf 'system\tvariant\tpackage\tframing\tshare_over_10pct\tshare_over_3pct\tcanvas_share_over_10pct\tmae\tlargest_region\tarea_px\teditor\tproduction\tdiff\tside\n' > "$out/metrics.tsv"
failures=0
for cell in "${cells[@]}"; do
  system="${cell%%:*}"; only=""; [ "$cell" != "$system" ] && only="${cell#*:}"
  bezels="$root/config/systems/$system/bezels.json"
  for variant in $(jq -r '.variants[].id' "$bezels"); do
    [ -z "$only" ] || [ "$variant" = "$only" ] || continue
    package="$(jq -r --arg id "$variant" '.variants[] | select(.id == $id) | .bezel' "$bezels")"
    name="$system-$variant"; productionLog="$work/$name-production.log"
    wide=""; [ "$(jq -r --arg id "$variant" '.variants[] | select(.id == $id) | .output // ""' "$bezels")" = widescreen ] && wide="1.777778"  # a variant for widescreen output is drawn on a 16:9 picture
    chosen="$(chosen_variant "$bezels" "$variant")"  # the Wii's tv_wide is reached through tv: render-env never selects it by name
    [ -n "$chosen" ] || { echo "$name: no variant lists $variant under outputs, so no launch reaches it"; failures=$((failures + 1)); continue; }
    editorImage="$out/$name-editor.png"; productionImage="$out/$name-production.png"; diffImage="$out/$name-diff.png"; sideImage="$out/$name-side.png"
    if ! production "$system" "$chosen" "$productionImage" "$wide" "$productionLog"; then
      echo "$name: production render FAILED (see $productionLog)"; failures=$((failures + 1)); continue
    fi
    drawn="$(drawn_package "$productionLog")"
    if [ "$drawn" != "$package" ]; then  # the renderer drew another package than the one previewed: no comparison means anything
      framing="PACKAGE DIFFERS: production drew ${drawn:-nothing} through $chosen, the variant declares $package"
      echo "$name: $framing (see $productionLog)" >&2; failures=$((failures + 1))
      printf '%s\t%s\t%s\t%s\tn/a\tn/a\tn/a\tn/a\tn/a\tn/a\tn/a\t%s\tn/a\tn/a\n' "$system" "$variant" "$package" "$framing" "$productionImage" >> "$out/metrics.tsv"
      rows+="<tr><td>$system</td><td>$variant</td><td>$package</td><td>$framing</td><td>n/a</td><td>n/a</td><td>n/a</td><td>n/a</td><td>n/a</td><td><a href=\"$name-production.png\">production</a></td></tr>"
      continue
    fi
    layout="$(jq -r '.layout // "fixed"' "$root/config/bezels/$package/bezel.json")"
    if [ "$(jq --arg id "$package" 'any(.[]; .id == $id)' "$work/packages.json")" != true ]; then
      framing="no editor view (layout $layout)"
      panel "$productionImage" "production · $name" "$work/production-panel.miff"
      magick -size "$((width / 2))x$((height / 2 + 26))" xc:'#16171a' -font DejaVu-Sans -pointsize 15 -fill '#9aa0aa' -gravity center -annotate +0+0 "the editor does not list $package (layout $layout)" "$work/missing-panel.miff" 2>>"$work/magick.log"
      magick "$work/missing-panel.miff" "$work/production-panel.miff" +append "PNG24:$sideImage"
      echo "$name: $framing"
      printf '%s\t%s\t%s\t%s\tn/a\tn/a\tn/a\tn/a\tn/a\tn/a\tn/a\t%s\tn/a\t%s\n' "$system" "$variant" "$package" "$framing" "$productionImage" "$sideImage" >> "$out/metrics.tsv"
      rows+="<tr><td>$system</td><td>$variant</td><td>$package</td><td>$framing</td><td>n/a</td><td>n/a</td><td>n/a</td><td>n/a</td><td>n/a</td><td><a href=\"$name-side.png\"><img loading=\"lazy\" src=\"$name-side.png\"></a></td></tr>"
      continue
    fi
    rendered="$(rendered_rect "$productionLog")"
    cardParameter=""; [ "$card" = test ] || cardParameter="&card=$card"
    url="http://127.0.0.1:$port/?capture=${width}x${height}${cardParameter}&preview=$system:$variant${placement:+&placement=$placement}#$package"
    if ! capture "$url" "$work/editor.png"; then
      echo "$name: editor capture FAILED ($url)"; failures=$((failures + 1)); continue
    fi
    magick "$work/editor.png" -alpha off "PNG24:$editorImage"
    framed="$(sed -n 's/.*semu-capture canvas \(-\{0,1\}[0-9]*\),\(-\{0,1\}[0-9]*\),\([0-9]*\),\([0-9]*\).*/\1 \2 \3 \4/p' "$captureLog" | tail -1)"  # the editor frames the canvas itself and logs where
    framing="editor framed $framed, production ${rendered:-nothing}"; placed="${rendered:-$framed}"
    if [ -n "$rendered" ] && [ -n "$framed" ]; then
      worst=0; read -r renderedX renderedY renderedWidth renderedHeight <<<"$rendered"; read -r framedX framedY framedWidth framedHeight <<<"$framed"
      for delta in $((renderedX - framedX)) $((renderedY - framedY)) $((renderedWidth - framedWidth)) $((renderedHeight - framedHeight)); do delta="${delta#-}"; [ "$delta" -gt "$worst" ] && worst="$delta"; done
      [ "$worst" -le 1 ] && framing="editor framing matches production ($rendered)" || framing="FRAMING DIFFERS by $worst px: editor $framed, production $rendered"
    fi
    [ "$layout" = fixed ] || { placed="0 0 $width $height"; framing="computed layout $layout: the whole screen, lanes from RendererLayout.computed"; }
    metrics="$(compare "$editorImage" "$productionImage" "$diffImage" "$placed")" || { echo "$name: comparison FAILED"; failures=$((failures + 1)); continue; }
    read -r share subtle inside mae region area <<<"$metrics"
    if [ "$share" != 0.000 ] || [ "${framing#FRAMING DIFFERS}" != "$framing" ]; then echo "$name: DIFFERS ($share% over 10%; $framing)" >&2; failures=$((failures + 1)); fi  # the editor must show what the renderer draws: a framing or pixel difference fails the run
    panel "$editorImage" "editor · $name" "$work/editor-panel.miff"
    panel "$productionImage" "production · $name" "$work/production-panel.miff"
    panel "$diffImage" "difference x3 · $share% over 10% · largest $region" "$work/diff-panel.miff"
    magick "$work/editor-panel.miff" "$work/production-panel.miff" "$work/diff-panel.miff" +append "PNG24:$sideImage"
    echo "$name: $share% of pixels differ by more than 10% ($subtle% by more than 3%, $inside% of the canvas by more than 10%), MAE $mae, largest region $region ($area px); $framing"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$system" "$variant" "$package" "$framing" "$share" "$subtle" "$inside" "$mae" "$region" "$area" "$editorImage" "$productionImage" "$diffImage" "$sideImage" >> "$out/metrics.tsv"
    rows+="<tr><td>$system</td><td>$variant</td><td>$package</td><td>$framing</td><td>$share%</td><td>$subtle%</td><td>$inside%</td><td>$mae</td><td>$region<br>$area px</td><td><a href=\"$name-side.png\"><img loading=\"lazy\" src=\"$name-side.png\"></a><br><a href=\"$name-editor.png\">editor</a> · <a href=\"$name-production.png\">production</a> · <a href=\"$name-diff.png\">diff</a></td></tr>"
  done
done

{
  cat <<'HEAD'
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Editor Sync</title>
<style>:root{--bg:#16171a;--panel:#1f2126;--line:#2c2f36;--text:#e8e8e8;--muted:#9aa0aa}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.45 system-ui,sans-serif}main{max-width:1500px;margin:0 auto;padding:16px}table{border-collapse:collapse;width:100%;font-size:12px}td,th{border-bottom:1px solid var(--line);padding:6px;text-align:left;vertical-align:top}td img{width:720px;max-width:100%;display:block;border-radius:4px}a{color:#7cb7ff}p{color:var(--muted)}</style></head><body><main><h1>Bezel editor against the renderer</h1>
HEAD
  echo "<p>$size, card $card${placement:+, placement $placement}, revision $(git -C "$root" rev-parse --short HEAD)$(git -C "$root" diff --quiet || echo ' with local changes'), $(date -u +%Y-%m-%dT%H:%MZ). Production: build/render-host fed by semu render-env, shader off. Editor: semu bezel edit ?capture=$size in headless Chrome. Share: pixels whose largest channel differs by more than 10% (or 3%) after a 1 px blur of both; canvas share: the same inside the canvas rectangle only; MAE on the raw images; region: the largest connected differing area (red box in the diff, which is the raw difference times three). Failures: $failures.</p>"
  echo "<table><tr><th>system</th><th>variant</th><th>package</th><th>framing</th><th>share &gt;10%</th><th>share &gt;3%</th><th>canvas share &gt;10%</th><th>MAE</th><th>largest region</th><th>editor · production · difference</th></tr>$rows</table></main></body></html>"
} > "$out/index.html"
echo "editor-sync: $out/index.html ($failures failures)"
exit "$failures"
