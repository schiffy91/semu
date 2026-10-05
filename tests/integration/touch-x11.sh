#!/usr/bin/env bash
# The right trackpad on RetroArch's DS and 3DS routes, checked on Linux on a private Xvfb: xdotool
# moves and clicks the X pointer (XTest, the way Steam's trackpad mouse reaches Game Mode's
# Xwayland, by relative motion from the top left as inject.sh does on the Deck) over a real
# RetroArch with Semu's renderer, the synthetic core standing in for the Azahar core (n3ds) and the
# melonDS core (nds) under their library names. The launch is exactly
# `semu launch retroarch` (its --print-plan argv, environment and written files) plus --verbose.
# Asserts, per system, from the bottom screen's drawn rectangle in semu-render-evidence.log:
#   taps at 5, 50 and 95 percent across it, half way down, reach the core within 1 percent of
#   where that core reads them (3DS: 0.1 + 0.8 f across its 400-wide frame; DS: f), and 0.75 down;
#   a tap on the bezel presses nothing; RetroArch loads Semu's remap file for that library name;
#   Semu's whole arrow is drawn with its tip where the pointer moved 1.5 s after the move (xwd sees
#   it, as it is drawn into the frame; tests/deck/cursor-arrow.sh judges it) and is gone 5 s after.
#   With SHADER=none the card's pixels reach the screen as they are, so the picture is also checked
#   where the receipt says it is drawn: each screen's outermost drawn ring is the card's white border
#   and the ring just outside it is not (a tap on the receipt's rectangle is a tap on the picture).
# SIZE=WxH sizes the screen (default 1280x800, the Deck). CASES lists the sessions as
# SYSTEM:BEZEL_VARIANT:PLACEMENT (visual.systems.<id>.bezel_variant and .placement; empty for the
# system's own default), e.g. CASES="nds:vertical:game n3ds:main_right:"; the default is
# "n3ds:: nds::". SHADER sets every session's shader_variant (empty: the system's default).
# On a Mac it runs inside the podman VM (x86_64 under Rosetta, the release builder's Nix store) and
# never touches the Mac display. Scratch lives in mktemp -d directories, the container is left
# exited, and nothing is removed.
#
#   touch-x11.sh OUT_DIR            # on the Mac: builds this checkout's tracked files, then runs
#   touch-x11.sh --inside OUT_DIR   # in Linux with nix: OUT/<label>.result, captures, OUT/result
set -eu
image=docker.io/nixos/nix:latest
if [ "${1:-}" != "--inside" ]; then
  out="$(mkdir -p "${1:?usage: touch-x11.sh OUT_DIR}" && cd "$1" && pwd -P)"
  repository="$(cd "$(dirname "$0")/../.." && pwd -P)"
  [ "$(podman machine inspect --format '{{.State}}')" = running ] || podman machine start
  podman machine ssh 'test -e /proc/sys/fs/binfmt_misc/rosetta || { sudo touch /etc/containers/enable-rosetta && sudo systemctl start rosetta-activation.service; }'
  name="semu-touch-x11-$(date +%Y%m%d%H%M%S)-$$"  # left behind exited; the shell's pid keeps two runs started in one second apart
  echo "container $name, results in $out"
  exec podman run --name "$name" --platform linux/amd64 --privileged \
    -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix -v "$repository":/src:ro -v "$out":/out \
    -e SIZE="${SIZE:-1280x800}" -e CASES="${CASES:-n3ds:: nds::}" -e SHADER="${SHADER:-}" \
    -e NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0" "$image" bash /src/tests/integration/touch-x11.sh --inside /out
fi
out="$2"
git config --global --add safe.directory '*'
flake() { nix build --no-link --print-out-paths "git+file:///src#$1" 2>>"$out/nix.log" | tail -1; }
package() { nix build --no-link --inputs-from /src "nixpkgs#$1" >>"$out/nix.log" 2>&1 && nix eval --raw --inputs-from /src "nixpkgs#$1.outPath"; }
echo "building (log: $out/nix.log)"
cli="$(flake packages.x86_64-linux.semu-cli)"
retroarch="$(flake packages.x86_64-linux.retroarch.unwrapped || true)"
[ -x "$retroarch/bin/retroarch" ] || retroarch="$(flake packages.x86_64-linux.retroarch)"
core="$(flake checks.x86_64-linux.synthetic-core)"
renderer="$(flake packages.x86_64-linux.semu-renderer)"
assets="$(flake packages.x86_64-linux.asset-root)"
mesa="$(package mesa)"; xvfb="$(package xvfb)"; xdotool="$(package xdotool)"; xwd="$(package xwd)"
magick="$(package imagemagick)/bin/magick"; jq="$(package jq)/bin/jq"; awk="$(package gawk)/bin/awk"
for tool in "$cli/lib/semu/semu-btrc" "$retroarch/bin/retroarch" "$core/lib/retroarch/cores/synthetic_libretro.so" "$renderer/lib/libsemurenderer.so" "$assets/share" "$xvfb/bin/Xvfb"; do
  [ -e "$tool" ] || { echo "FAIL: missing $tool" | tee "$out/result"; exit 1; }
done
printf 'cli=%s\nretroarch=%s\ncore=%s\nrenderer=%s\nassets=%s\n' "$cli" "$retroarch" "$core" "$renderer" "$assets" | tee "$out/paths"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe LIBGL_DRIVERS_PATH="$mesa/lib/dri" __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_DIRS="$mesa/share/glvnd/egl_vendor.d" LD_LIBRARY_PATH="$mesa/lib" SDL_AUDIODRIVER=dummy
unset WAYLAND_DISPLAY
display=:93
size="${SIZE:-1280x800}"; width="${size%x*}"; height="${size#*x}"
"$xvfb/bin/Xvfb" "$display" -screen 0 ${width}x${height}x24 >"$out/xvfb.log" 2>&1 & xvfb_pid=$!
sleep 2
x() { DISPLAY="$display" "$xdotool/bin/xdotool" "$@"; }
glide() { x mousemove_relative -- -4000 -4000; sleep 0.05; x mousemove_relative -- "$1" "$2"; }  # X Y: as Steam's trackpad mouse moves the pointer (and inject.sh on the Deck): parked at the top left, then relative
shot() { DISPLAY="$display" "$xwd/bin/xwd" -root -silent | "$magick" xwd:- "$1"; }
presses() { grep -c 'synthetic: pointer press' "$1" || true; }
inside() {  # X Y LEFT TOP W H: is the point on that rectangle
  [ "$1" -ge "$3" ] && [ "$1" -lt $(($3 + $5)) ] && [ "$2" -ge "$4" ] && [ "$2" -lt $(($4 + $6)) ]
}
summed() {  # IMAGE LEFT TOP W H: the sum over that rectangle of each pixel's darkest channel (0-255), 0 for an empty one
  [ "$4" -gt 0 ] && [ "$5" -gt 0 ] || { echo 0; return; }
  "$magick" "$1" -crop "${4}x${5}+${2}+${3}" +repage -separate -evaluate-sequence min -format "%[fx:mean]" info: \
    | "$awk" -v area=$(($4 * $5)) '{ printf "%d\n", $1 * 255 * area + 0.5 }'  # fx prints large sums in exponent form, which bash cannot add
}
ring() {  # IMAGE LEFT TOP W H: the mean darkest channel along the rectangle's one-pixel outline, or "off" when it leaves the screen
  if [ "$2" -lt 0 ] || [ "$3" -lt 0 ] || [ $(($2 + $4)) -gt "$width" ] || [ $(($3 + $5)) -gt "$height" ]; then echo off; return; fi
  local whole inner
  whole="$(summed "$1" "$2" "$3" "$4" "$5")"; inner="$(summed "$1" $(($2 + 1)) $(($3 + 1)) $(($4 - 2)) $(($5 - 2)))"
  echo $(( (whole - inner) / (2 * $4 + 2 * $5 - 4) ))
}

session() {  # $1 system, $2 core file, $3 library name, $4 the core's frame width over the touch screen's, $5 bezel variant, $6 placement
  system="$1"; corefile="$2"; library="$3"; ratio="$4"; variant="$5"; placement="$6"
  label="$system-$corefile${variant:+-$variant}${placement:+-$placement}"
  printf '%s %s %s\n' "$label" "$library" "$ratio" >> "$out/labels"
  root="$(mktemp -d)"
  mkdir -p "$root/home" "$root/assets/bin" "$root/assets/lib/retroarch/cores" "$root/roms/$system"
  ln -s "$assets/share" "$root/assets/share"
  ln -s "$retroarch/bin/retroarch" "$root/assets/bin/retroarch"
  ln -s "$core/lib/retroarch/cores/synthetic_libretro.so" "$root/assets/lib/retroarch/cores/${corefile}_libretro.so"
  ln -s "$renderer/lib/libsemurenderer.so" "$root/assets/lib/libsemurenderer.so"
  printf 'semu synthetic content\n' > "$root/roms/$system/pattern.semu"
  settings="$("$jq" -n -c --arg root "$root" --arg system "$system" --arg variant "$variant" --arg placement "$placement" --arg shader "${SHADER:-}" \
    '{paths: {roms: ($root + "/roms"), state_root: ($root + "/state"), content_root: ($root + "/content")},
      visual: {systems: {($system): ({} + (if $variant == "" then {} else {bezel_variant: $variant} end)
        + (if $placement == "" then {} else {placement: $placement} end) + (if $shader == "" then {} else {shader_variant: $shader} end))}}}')"
  HOME="$root/home" "$cli/lib/semu/semu-btrc" launch retroarch --system "$system" --core "$corefile" --rom pattern.semu --project /src/config \
    --asset-root "$root/assets" --settings-json "$settings" --semu-home "$root/home/semu" --target linux-desktop --print-plan > "$out/$label.plan.json"
  [ -z "$("$jq" -r .error "$out/$label.plan.json")" ] || { echo "$label: plan error $("$jq" -r .error "$out/$label.plan.json")"; return 1; }
  log="$out/$label.log"
  x mousemove 4 4
  (
    while IFS= read -r entry; do export "$entry"; done < <("$jq" -r '.environment[]' "$out/$label.plan.json")
    "$jq" -r '.argv[]' "$out/$label.plan.json" > "$root/argv"
    set --
    while IFS= read -r argument; do set -- "$@" "$argument"; done < "$root/argv"
    executable="$1"; shift
    HOME="$root/home" DISPLAY="$display" SEMU_RENDER_DEBUG=1 SEMU_RENDER_CAPTURE_FRAME=30 SEMU_SYNTHETIC_LIBRARY_NAME="$library" exec "$executable" --verbose "$@"
  ) > "$log" 2>&1 &
  game=$!
  evidence="$root/state/retroarch/semu-render-evidence.log"
  for _ in $(seq 1 240); do grep -q 'surface1_content=' "$evidence" 2>/dev/null && break; kill -0 "$game" 2>/dev/null || break; sleep 0.5; done
  receipt="$(grep -m1 'surface1_content=' "$evidence" 2>/dev/null || true)"
  [ -n "$receipt" ] || { echo "$label: no receipt with the touch surface"; tail -30 "$log"; kill -TERM "$game" 2>/dev/null; return 1; }
  rect="$(printf '%s\n' "$receipt" | tr ' ' '\n' | grep '^surface1_content=' | cut -d= -f2)"
  top_rect="$(printf '%s\n' "$receipt" | tr ' ' '\n' | grep '^surface0_content=' | cut -d= -f2)"
  frame_height="$(printf '%s\n' "$receipt" | tr ' ' '\n' | grep '^framebuffer_size=' | cut -d= -f2 | cut -dx -f2)"
  IFS=, read -r gl_left gl_bottom touch_width touch_height <<EOF
$rect
EOF
  IFS=, read -r top_gl_left top_gl_bottom top_width top_height <<EOF
$top_rect
EOF
  touch_left="$gl_left"; touch_top=$((frame_height - gl_bottom - touch_height))
  top_left="$top_gl_left"; top_top=$((frame_height - top_gl_bottom - top_height))
  x search --onlyvisible --name . getwindowgeometry %@ > "$out/$label.windows" 2>&1 || true
  sleep 3
  shot "$out/$label-0-running.png"
  drawn_touch="$(ring "$out/$label-0-running.png" "$touch_left" "$touch_top" "$touch_width" "$touch_height"):$(ring "$out/$label-0-running.png" $((touch_left - 1)) $((touch_top - 1)) $((touch_width + 2)) $((touch_height + 2)))"
  drawn_top="$(ring "$out/$label-0-running.png" "$top_left" "$top_top" "$top_width" "$top_height"):$(ring "$out/$label-0-running.png" $((top_left - 1)) $((top_top - 1)) $((top_width + 2)) $((top_height + 2)))"
  middle=$((touch_top + touch_height / 2))
  : > "$out/$label.taps"
  for percent in 5 50 95; do
    tap_x=$((touch_left + touch_width * percent / 100))
    before="$(presses "$log")"
    glide "$tap_x" "$middle"; sleep 0.5; x mousedown 1; sleep 0.8; x mouseup 1; sleep 1.2
    line="$(grep 'synthetic: pointer press' "$log" | tail -1)"
    [ "$(presses "$log")" -gt "$before" ] || line="none"
    printf '%s %s %s\n' "$percent" "$tap_x,$middle" "$line" >> "$out/$label.taps"
  done
  bezel=""
  for candidate in "$((touch_left - 25)) $middle" "$((touch_left + touch_width + 25)) $middle" "$((touch_left + touch_width / 2)) $((touch_top - 25))" "$((touch_left + touch_width / 2)) $((touch_top + touch_height + 25))"; do
    set -- $candidate
    [ "$1" -ge 0 ] && [ "$1" -lt "$width" ] && [ "$2" -ge 0 ] && [ "$2" -lt "$height" ] || continue
    inside "$1" "$2" "$touch_left" "$touch_top" "$touch_width" "$touch_height" && continue
    inside "$1" "$2" "$top_left" "$top_top" "$top_width" "$top_height" && continue
    bezel="$1,$2"; break
  done
  bezel_presses=unknown
  if [ -n "$bezel" ]; then
    before="$(presses "$log")"
    glide "${bezel%,*}" "${bezel#*,}"; sleep 0.5; x mousedown 1; sleep 0.8; x mouseup 1; sleep 1.2
    bezel_presses=$(($(presses "$log") - before))
  fi
  glide 40 44; moved="$(date +%s%3N)"  # a static corner of the plate, captured as input-check.sh does on the Deck
  sleep 1.5
  shot "$out/$label-1-cursor.png"
  while [ $(( $(date +%s%3N) - moved )) -lt 5000 ]; do sleep 0.1; done
  shot "$out/$label-2-idle.png"
  arrow_shown=0; arrow_idle=0
  shown_text="$(PATH="$(dirname "$magick"):$PATH" bash /src/tests/deck/cursor-arrow.sh "$out/$label-1-cursor.png" 40 44)" || arrow_shown=$?
  idle_text="$(PATH="$(dirname "$magick"):$PATH" bash /src/tests/deck/cursor-arrow.sh "$out/$label-2-idle.png" 40 44)" || arrow_idle=$?
  "$magick" "$out/$label-1-cursor.png" -crop 64x80+30+34 +repage -scale 400% "$out/$label-1-cursor-zoom.png"
  alive=yes; kill -0 "$game" 2>/dev/null || alive=no
  kill -TERM "$game" 2>/dev/null || true
  wait "$game" 2>/dev/null || true
  {
    echo "label=$label"
    echo "alive=$alive"
    echo "touch_rect=$touch_left,$touch_top,${touch_width}x$touch_height top_rect=$top_left,$top_top,${top_width}x$top_height frame_height=$frame_height"
    echo "drawn_touch=$drawn_touch drawn_top=$drawn_top"  # the darkest channel along each screen's outermost drawn ring : along the ring outside it
    echo "ratio=$ratio"
    "$awk" '{ if ($3 == "none") { printf "tap_%s=none\n", $1; next } split($8, core, ","); printf "tap_%s=%s,%s\n", $1, core[1], core[2] }' "$out/$label.taps"  # 5 X,Y synthetic: pointer press RAW normalized NX,NY
    echo "bezel_tap=$bezel bezel_presses=$bezel_presses"
    echo "bezel_logged=$(grep -c 'semu-retroarch: touch .* -> no surface' "$log" || true)"
    echo "bridge_presses=$(grep -c 'semu-retroarch: touch .* -> surface 1' "$log" || true)"
    echo "remap=$(grep -o "Core-specific remap found at \"[^\"]*\"" "$log" | head -1)"
    echo "arrow_shown=$arrow_shown arrow_idle=$arrow_idle"
    echo "arrow_text=$shown_text / $idle_text" | tr " " "_"
    echo "renderer_cursor=$(grep -o 'semu-renderer: cursor [a-z]* [0-9,-]*' "$log" | head -6 | cut -d" " -f3,4 | tr " \n" ":;")"
  } > "$out/$label.result"
  grep 'semu-retroarch: touch\|synthetic: pointer' "$log" > "$out/$label.touch-lines" || true
  cat "$out/$label.result"
}

expect_near() {  # LABEL ACTUAL EXPECTED: within 0.01
  if "$awk" -v actual="$2" -v expected="$3" 'BEGIN { difference = actual - expected; if (difference < 0) difference = -difference; exit !(actual != "" && difference <= 0.01) }'; then
    echo "ok   $1 ($2, want $3)"
  else
    echo "FAIL $1: got ${2:-nothing}, want $3 within 0.01"
  fi
}
expect() { if [ "$2" = "$3" ]; then echo "ok   $1 ($2)"; else echo "FAIL $1: got $2, want $3"; fi; }
value() { "$awk" -v key="$2" '{ for (field = 1; field <= NF; field++) if (index($field, key "=") == 1) { print substr($field, length(key) + 2); exit } }' "$out/$1.result"; }

drawn() {  # LABEL SCREEN RINGS: the outermost drawn ring is the card's white border, the ring outside it is not
  local inner="${3%%:*}" outer="${3#*:}"
  if [ "$inner" != off ] && [ "$inner" -ge 245 ] && { [ "$outer" = off ] || [ "$outer" -lt 200 ]; }; then
    echo "ok   $1: the $2 screen is drawn on its receipt's rectangle (ring $inner, outside $outer)"
  else
    echo "FAIL $1: the $2 screen is not drawn on its receipt's rectangle (ring ${inner:-nothing}, outside ${outer:-nothing}; want 245 or more, then under 200)"
  fi
}

: > "$out/labels"
for case in ${CASES:-n3ds:: nds::}; do
  IFS=: read -r system variant placement <<EOF
$case
EOF
  case "$system" in
    n3ds) session n3ds azahar Azahar 0.8 "$variant" "$placement" || echo "FAIL: the n3ds $case session did not run" >> "$out/sessions" ;;
    nds) session nds melonds melonDS 1.0 "$variant" "$placement" || echo "FAIL: the nds $case session did not run" >> "$out/sessions" ;;
    *) echo "FAIL: case $case names no DS or 3DS system" >> "$out/sessions" ;;
  esac
done
kill "$xvfb_pid" 2>/dev/null || true

{
  cat "$out/sessions" 2>/dev/null || true
  while read -r label library ratio; do
    [ -f "$out/$label.result" ] || continue
    if [ "${SHADER:-}" = none ]; then
      drawn "$label" touch "$(value "$label" drawn_touch)"
      drawn "$label" top "$(value "$label" drawn_top)"
    fi
    for percent in 5 50 95; do
      tap="$(value "$label" "tap_$percent")"
      want_x="$("$awk" -v ratio="$ratio" -v percent="$percent" 'BEGIN { printf "%.4f", (1 - ratio) / 2 + ratio * percent / 100 }')"
      expect_near "$label: a tap at $percent% across the touch screen, core x" "${tap%,*}" "$want_x"
      expect_near "$label: the same tap half way down, core y" "${tap#*,}" 0.75
    done
    expect "$label: a tap on the bezel presses nothing" "$(value "$label" bezel_presses)" 0
    expect "$label: the bridge logged that bezel press as no surface" "$(value "$label" bezel_logged)" 1
    expect "$label: the bridge logged the three screen taps" "$(value "$label" bridge_presses)" 3
    case "$(grep '^remap=' "$out/$label.result")" in
      *"/remaps/$library/$library.rmp\"") echo "ok   $label: RetroArch loads Semu's remaps/$library/$library.rmp" ;;
      *) echo "FAIL $label: no remaps/$library/$library.rmp load in RetroArch's log" ;;
    esac
    expect "$label: Semu's whole arrow at the pointer 1.5 s after a relative move (cursor-arrow.sh: $(value "$label" arrow_text))" "$(value "$label" arrow_shown)" 0
    expect "$label: and gone 5 s after it" "$(value "$label" arrow_idle)" 1
    expect "$label: RetroArch ran to the end" "$(value "$label" alive)" yes
  done < "$out/labels"
} | tee "$out/result"
grep -q '^FAIL' "$out/result" && { echo "touch-x11: FAIL" | tee -a "$out/result"; exit 1; }
echo "touch-x11: PASS" | tee -a "$out/result"
