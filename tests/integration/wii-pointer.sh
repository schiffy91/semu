#!/usr/bin/env bash
# Does the Wii Remote IR follow the right-trackpad pointer over the bezelled picture? On Linux in
# the podman VM under a private Xvfb with openbox (software GL), one real Wii game is launched
# through `semu launch dolphin` with SEMU_RENDER_DEBUG. xdotool moves the X pointer the way Steam's
# trackpad mouse does (parked at the top left, then relative) to the composed picture's four
# corners, its centre and a point on the bezel, read from the renderer's SEMU_RENDER_DEBUG line.
# libsemupreload's XIQueryPointer logs "semu-preload: pointer X,Y -> X',Y'" for each new point;
# each must land where Dolphin drew that point of its 4:3 letterbox (the bezel point clamped to the
# picture's corner), within 1 px. A capture after each move shows the game's own IR pointer there
# when the title draws one (xwd never shows the X arrow, which Dolphin.ini CursorVisibility = 0
# blanks; Dolphin.ini is copied to OUT to read). ROMs are mounted read-only, scratch lives in
# mktemp -d, the container is left exited, nothing is removed, and the Mac display is never used.
#
#   wii-pointer.sh OUT_DIR ROM_PATH   # on the Mac
#   wii-pointer.sh --inside OUT_DIR   # in Linux, reading OUT_DIR/rom
#
# WAIT seconds after the first composed frame before the sweep (default 150); PRESSES Return
# presses 25 s apart before the sweep, to pass a title's warning screen (default 0; the keyboard Return is
# both Wii A and HOME in the profile, so a press also opens the HOME menu). OUT/sweep.png holds a
# 200 px crop around each target with a red cross on it: the game's pointer should sit at the cross.
set -eu
image=docker.io/nixos/nix:latest
if [ "${1:-}" != "--inside" ]; then
  out="$(mkdir -p "${1:?usage: wii-pointer.sh OUT_DIR ROM_PATH}" && cd "$1" && pwd -P)"
  rom="${2:?usage: wii-pointer.sh OUT_DIR ROM_PATH}"
  repository="$(cd "$(dirname "$0")/../.." && pwd -P)"
  [ "$(podman machine inspect --format '{{.State}}')" = running ] || podman machine start
  basename "$rom" > "$out/rom"
  name="semu-wii-pointer-$(date +%Y%m%d%H%M%S)"  # left behind exited
  echo "container $name, results in $out"
  exec podman run --name "$name" --platform linux/amd64 --privileged --shm-size=4g -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix \
    -v "$repository":/src:ro -v "$out":/out -v "$rom:/roms/wii/$(basename "$rom"):ro" -e WAIT="${WAIT:-150}" -e PRESSES="${PRESSES:-0}" \
    -e NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0" "$image" bash /src/tests/integration/wii-pointer.sh --inside /out
fi
out="$2"
git config --global --add safe.directory '*'
package() { nix build --no-link --inputs-from /src "nixpkgs#$1" >>"$out/nix.log" 2>&1 && nix eval --raw --inputs-from /src "nixpkgs#$1.outPath"; }
echo "building (log: $out/nix.log)"
bundle="$(nix build --no-link --print-out-paths "git+file:///src#packages.x86_64-linux.semu" 2>>"$out/nix.log" | tail -1)"
mesa="$(package mesa)"; xvfb="$(package xvfb)"; xdotool="$(package xdotool)"; xwd="$(package xwd)"; magick="$(package imagemagick)/bin/magick"; openbox="$(package openbox)"
awk="$(package gawk)/bin/awk"  # the nix image has no awk or sed
[ -x "$bundle/bin/semu" ] || { echo "FAIL: no bundle" | tee "$out/result"; exit 1; }
echo "bundle $bundle" | tee "$out/bundle"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe LIBGL_DRIVERS_PATH="$mesa/lib/dri" __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_DIRS="$mesa/share/glvnd/egl_vendor.d" LD_LIBRARY_PATH="$mesa/lib"
export SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy QT_QPA_PLATFORM=xcb PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none
unset WAYLAND_DISPLAY
display=:94
width=1280; height=800
"$xvfb/bin/Xvfb" "$display" -screen 0 ${width}x${height}x24 >"$out/xvfb.log" 2>&1 & xvfb_pid=$!
sleep 2
DISPLAY="$display" "$openbox/bin/openbox" --sm-disable >"$out/openbox.log" 2>&1 &
sleep 2
x() { DISPLAY="$display" "$xdotool/bin/xdotool" "$@"; }
shot() { DISPLAY="$display" "$xwd/bin/xwd" -root -silent | "$magick" xwd:- "$1"; }
glide() { x mousemove_relative -- -4000 -4000; sleep 0.05; x mousemove_relative -- "$1" "$2"; }  # as Steam's trackpad mouse moves the pointer

rom="/roms/wii/$(cat "$out/rom")"
root="$(mktemp -d)"
mkdir -p "$root/home" "$root/emulation"
settings="{\"paths\":{\"roms\":\"/roms\",\"state_root\":\"$root/state\",\"content_root\":\"$root/content\",\"emulation_root\":\"$root/emulation\",\"bios\":\"$root/emulation\"}}"
HOME="$root/home" DISPLAY="$display" SEMU_RENDER_DEBUG=1 "$bundle/bin/semu" launch dolphin --system wii --rom "$rom" \
  --settings-json "$settings" --semu-home "$root/home/semu" > "$out/dolphin.log" 2>&1 &
launcher=$!
composed() {  # the newest full-screen composition's picture, from the renderer's debug line (every 120 frames): "LEFT BOTTOM WIDTH HEIGHT"
  local line
  line="$(grep "^semu-renderer: frame [0-9]* bezel [^ ]* fb ${width}x$height lane0 out " "$out/dolphin.log" 2>/dev/null | tail -1)"
  [[ "$line" =~ \ lane0\ out\ ([0-9]+),([0-9]+)\ ([0-9]+)x([0-9]+)\  ]] && echo "${BASH_REMATCH[1]} ${BASH_REMATCH[2]} ${BASH_REMATCH[3]} ${BASH_REMATCH[4]}"
  return 0
}
for _ in $(seq 1 600); do [ -n "$(composed)" ] && break; kill -0 "$launcher" 2>/dev/null || break; sleep 1; done
for window in $(x search --onlyvisible --name . 2>/dev/null); do x windowmove "$window" 0 0 windowsize "$window" "$width" "$height" 2>/dev/null || true; done
sleep "${WAIT:-150}"
read -r left bottom picture_width picture_height <<EOF
$(composed)
EOF
[ -n "${picture_height:-}" ] || { echo "FAIL: no full-screen composition" | tee "$out/result"; tail -40 "$out/dolphin.log"; exit 1; }
top=$((height - bottom - picture_height))  # lanes count rows from the bottom
echo "picture=$left,$top ${picture_width}x$picture_height on ${width}x$height" | tee "$out/picture"
for window in $(x search --onlyvisible --name . 2>/dev/null); do x windowactivate --sync "$window" 2>/dev/null || true; done
glide $((left + picture_width / 2)) $((top + picture_height / 2)); sleep 2
for press in $(seq 1 "${PRESSES:-0}"); do
  shot "$out/0-before-press-$press.png"
  x key --delay 80 Return; sleep 25
done
cp "$root/state/dolphin/dolphin-user/Config/Dolphin.ini" "$out/Dolphin.ini" 2>/dev/null || true
cp "$root/state/dolphin/dolphin-user/Config/WiimoteNew.ini" "$out/WiimoteNew.ini" 2>/dev/null || true
expected() {  # X Y on the composed picture: where Dolphin drew it in its 4:3 letterbox, clamped
  "$awk" -v px="$1" -v py="$2" -v l="$left" -v t="$top" -v w="$picture_width" -v h="$picture_height" -v fw="$width" -v fh="$height" 'BEGIN {
      exact = fh * 4 / 3; edge = (fw - exact) / 2
      x0 = int(edge - 0.001); if (x0 < edge - 0.001) x0 = x0 + 1; x1 = int(edge + exact + 0.001)
      nx = (px - l) / w; ny = (py - t) / h
      if (nx < 0) nx = 0; if (nx > 1) nx = 1; if (ny < 0) ny = 0; if (ny > 1) ny = 1
      printf "%.1f %.1f", x0 + nx * (x1 - x0), ny * fh }'
}
at() {  # FX FY: the pixel of the composed picture at those fractions (1 is its last row or column)
  "$awk" -v fx="$1" -v fy="$2" -v l="$left" -v t="$top" -v w="$picture_width" -v h="$picture_height" 'BEGIN { printf "%d %d", l + int(fx * (w - 1) + 0.5), t + int(fy * (h - 1) + 0.5) }'
}
crop() {  # CAPTURE X Y CROP: 200 px around the target, a red cross on it
  local cx=$(($2 - 100)) cy=$(($3 - 100))
  [ "$cx" -ge 0 ] || cx=0; [ "$cy" -ge 0 ] || cy=0; [ "$cx" -le $((width - 200)) ] || cx=$((width - 200)); [ "$cy" -le $((height - 200)) ] || cy=$((height - 200))
  local mx=$(($2 - cx)) my=$(($3 - cy))
  "$magick" "$1" -crop "200x200+$cx+$cy" +repage -fill none -stroke red -draw "line $((mx - 8)),$my $((mx + 8)),$my" -draw "line $mx,$((my - 8)) $mx,$((my + 8))" -bordercolor black -border 2 "$4"
}
: > "$out/sweep"
failures=0
number=0
crops=()
for target in "0 0 top-left" "1 0 top-right" "0 1 bottom-left" "1 1 bottom-right" "0.05 0.05 near-top-left" "0.95 0.05 near-top-right" \
              "0.05 0.95 near-bottom-left" "0.95 0.95 near-bottom-right" "0.5 0.03 near-top" "0.5 0.97 near-bottom" "0.03 0.5 near-left" \
              "0.97 0.5 near-right" "0.25 0.5 quarter" "0.75 0.5 three-quarters" "0.5 0.5 centre" "bezel bezel bezel"; do
  set -- $target
  name="$3"
  if [ "$1" = bezel ]; then set -- $((left / 2)) $((top / 2)); else set -- $(at "$1" "$2"); fi
  number=$((number + 1))
  glide "$1" "$2"; sleep 4
  label="$(printf '%02d' "$number")-$name"
  shot "$out/$label.png"
  crop "$out/$label.png" "$1" "$2" "$out/crop-$label.png" && crops+=("$out/crop-$label.png")
  line="$(grep -a "semu-preload: pointer $1.0,$2.0 -> " "$out/dolphin.log" | tail -1 || true)"
  want="$(expected "$1" "$2")"
  got="${line##* -> }"; got="${got/,/ }"
  verdict=FAIL
  [ -n "$line" ] && "$awk" -v want="$want" -v got="$got" 'BEGIN { split(want, a, " "); split(got, b, " "); dx = a[1] - b[1]; dy = a[2] - b[2]; exit !(dx <= 1 && dx >= -1 && dy <= 1 && dy >= -1) }' && verdict=PASS
  [ "$verdict" = PASS ] || failures=$((failures + 1))
  printf '%s %s %s,%s want %s got %s\n' "$verdict" "$name" "$1" "$2" "$want" "${got:-nothing}" | tee -a "$out/sweep"
done
[ "${#crops[@]}" -gt 0 ] && "$magick" "${crops[@]:0:8}" +append "$out/sweep-1.png" && "$magick" "${crops[@]:8}" +append "$out/sweep-2.png" && "$magick" "$out/sweep-1.png" "$out/sweep-2.png" -append "$out/sweep.png" || true
grep -a -c 'semu-preload: pointer ' "$out/dolphin.log" > "$out/pointer-lines" || true
alive=yes; kill -0 "$launcher" 2>/dev/null || alive=no
kill -TERM "$launcher" 2>/dev/null || true
wait "$launcher" 2>/dev/null || true
kill "$xvfb_pid" 2>/dev/null || true
{ echo "alive_until_the_end=$alive failures=$failures"; grep -h '^CursorVisibility\|^IR/' "$out/Dolphin.ini" "$out/WiimoteNew.ini" 2>/dev/null | sort -u; } | tee "$out/result"
echo "wii-pointer: done; judge OUT/sweep.png and the captures in $out"
[ "$failures" = 0 ]
