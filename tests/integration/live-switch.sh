#!/usr/bin/env bash
# A bezel and shader choice switched live inside a real emulator, end to end: each case launches a
# game through `semu launch` on Linux in the podman VM (private Xvfb, openbox, software GL and
# Vulkan), types the radial's Next Bezel (Ctrl+Shift+B) and Next Shader (Ctrl+Shift+V) chords as
# XTest the way Steam does, and captures the toast and the switched picture; then opens the Semu
# menu to show the BEZEL and SHADER rows' values. The supervisor's X key source, the journal's
# select records, the renderer's variants file and the per-system save all take part.
# ROMs are mounted read-only, scratch lives in mktemp -d, the container is left exited and nothing
# is removed. Never touches the Mac display.
#
#   live-switch.sh OUT_DIR CASE...    # on the Mac; CASE is emulator:system:rom-path
#   live-switch.sh --inside OUT_DIR   # in Linux, reading OUT_DIR/cases
#
# SEMU_REV=<rev> builds that commit instead of this checkout's tracked files; SEMU_PS2_BIOS=DIR
# mounts a PS2 BIOS folder; WAIT seconds before the first chord (default 120); PLACEMENT=bezel
# shows a handheld's whole shell instead of the cropped integer picture. Each case writes
# OUT/<n>-<emulator>.result (actions seen, journal records, switch receipts, saved choices) and
# captures to judge by eye: three in the two seconds after each chord, as software GL draws slowly.
set -eu
image=docker.io/nixos/nix:latest
if [ "${1:-}" != "--inside" ]; then
  out="$(mkdir -p "${1:?usage: live-switch.sh OUT_DIR CASE...}" && cd "$1" && pwd -P)"
  shift
  repository="$(cd "$(dirname "$0")/../.." && pwd -P)"
  [ "$(podman machine inspect --format '{{.State}}')" = running ] || podman machine start
  mounts=()
  : > "$out/cases"
  for case in "$@"; do
    emulator="${case%%:*}"; rest="${case#*:}"; system="${rest%%:*}"; rom="${rest#*:}"
    printf '%s\t%s\t%s\n' "$emulator" "$system" "$(basename "$rom")" >> "$out/cases"
    mounts+=(-v "$rom:/roms/$system/$(basename "$rom"):ro")
  done
  [ -n "${SEMU_PS2_BIOS:-}" ] && mounts+=(-v "$SEMU_PS2_BIOS:/emulation/PCSX2/config/bios:ro")
  name="semu-live-switch-$(date +%Y%m%d%H%M%S)"  # left behind exited
  echo "container $name, results in $out"
  exec podman run --name "$name" --platform linux/amd64 --privileged --shm-size=4g -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix \
    -v "$repository":/src:ro -v "$out":/out "${mounts[@]}" -e SEMU_REV="${SEMU_REV:-}" -e WAIT="${WAIT:-120}" -e PLACEMENT="${PLACEMENT:-}" \
    -e NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0" "$image" bash /src/tests/integration/live-switch.sh --inside /out
fi
out="$2"
git config --global --add safe.directory '*'
source="git+file:///src${SEMU_REV:+?rev=$SEMU_REV}"
package() { nix build --no-link --inputs-from /src "nixpkgs#$1" >>"$out/nix.log" 2>&1 && nix eval --raw --inputs-from /src "nixpkgs#$1.outPath"; }
echo "building $source (log: $out/nix.log)"
bundle="$(nix build --no-link --print-out-paths "$source#packages.x86_64-linux.semu" 2>>"$out/nix.log" | tail -1)"
mesa="$(package mesa)"; xvfb="$(package xvfb)"; xdotool="$(package xdotool)"; xwd="$(package xwd)"; magick="$(package imagemagick)/bin/magick"; openbox="$(package openbox)"; jq="$(package jq)/bin/jq"
[ -x "$bundle/bin/semu" ] || { echo "FAIL: no bundle" | tee "$out/result"; exit 1; }
echo "bundle $bundle" | tee "$out/bundle"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe LIBGL_DRIVERS_PATH="$mesa/lib/dri" __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_DIRS="$mesa/share/glvnd/egl_vendor.d" LD_LIBRARY_PATH="$mesa/lib" VK_DRIVER_FILES="$(ls "$mesa"/share/vulkan/icd.d/lvp_icd*.json | head -1)"
printf "pcm.!default {\n  type null\n}\nctl.!default {\n  type hw\n  card 0\n}\n" > "$out/alsa-null.conf"  # cubeb (PCSX2) falls back to ALSA: a silent device, or a modal error hides the game
export SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy QT_QPA_PLATFORM=xcb ALSA_CONFIG_PATH="$out/alsa-null.conf" PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none
unset WAYLAND_DISPLAY
display=:92
width=1280; height=800
"$xvfb/bin/Xvfb" "$display" -screen 0 ${width}x${height}x24 >"$out/xvfb.log" 2>&1 & xvfb_pid=$!
sleep 2
DISPLAY="$display" "$openbox/bin/openbox" --sm-disable >"$out/openbox.log" 2>&1 &  # focus for Qt emulators, as gamescope gives it
sleep 2
x() { DISPLAY="$display" "$xdotool/bin/xdotool" "$@"; }
shot() { DISPLAY="$display" "$xwd/bin/xwd" -root -silent | "$magick" xwd:- "$1"; }
focus() {  # size every named window to the screen, then activate the last one, the game
  window=""
  for window in $(x search --onlyvisible --name . 2>/dev/null); do x windowmove "$window" 0 0 windowsize "$window" "$width" "$height" 2>/dev/null || true; done
  sleep 2
  x mousemove $((width / 2)) $((height / 2)) || true
  if [ -n "$window" ]; then x windowactivate --sync "$window" 2>/dev/null || true; x windowfocus --sync "$window" 2>/dev/null || true; fi
  [ -n "$window" ] || echo "live-switch: no window to focus yet"  # the captures and the result still record the case
}
press() { x key --delay 80 "$1"; }
burst() {  # PREFIX: the screen 0.3, 1 and 2 s after a chord, grabbed raw first so the times hold
  sleep 0.3; DISPLAY="$display" "$xwd/bin/xwd" -root -silent > "$1-a.xwd"
  sleep 0.7; DISPLAY="$display" "$xwd/bin/xwd" -root -silent > "$1-b.xwd"
  sleep 1.0; DISPLAY="$display" "$xwd/bin/xwd" -root -silent > "$1-c.xwd"
  for grab in a b c; do "$magick" "xwd:$1-$grab.xwd" "$1-$grab.png"; done
}

number=0
while IFS="$(printf '\t')" read -r emulator system rom; do
  number=$((number + 1))
  label="$number-$emulator-$system"
  root="$(mktemp -d)"
  mkdir -p "$root/home" "$root/emulation"
  visual=""; [ -n "${PLACEMENT:-}" ] && visual=",\"visual\":{\"systems\":{\"$system\":{\"placement\":\"$PLACEMENT\"}}}"
  settings="{\"paths\":{\"roms\":\"/roms\",\"state_root\":\"$root/state\",\"content_root\":\"$root/content\",\"emulation_root\":\"/emulation\",\"bios\":\"$root/emulation\"}$visual}"
  HOME="$root/home" DISPLAY="$display" SEMU_RENDER_DEBUG=1 "$bundle/bin/semu" launch "$emulator" --system "$system" --rom "$rom" \
    --settings-json "$settings" --semu-home "$root/home/semu" > "$out/$label.log" 2>&1 &
  launcher=$!
  sleep "${WAIT:-120}"
  focus
  sleep 3
  shot "$out/$label-1-before.png"
  press ctrl+shift+b; burst "$out/$label-2-bezel"  # the toast, while the new bezel decodes
  sleep 4; shot "$out/$label-3-bezel-after.png"
  press ctrl+shift+v; burst "$out/$label-4-shader"  # LOADING, then the plain toast once the chain is built
  sleep 6; shot "$out/$label-5-shader-after.png"
  press ctrl+m; sleep 3
  shot "$out/$label-6-menu.png"
  press ctrl+m; sleep 2
  journal="$root/state/$emulator/semu-render-actions.bin"
  alive=yes; kill -0 "$launcher" 2>/dev/null || alive=no
  kill -TERM "$launcher" 2>/dev/null || true
  status=0; wait "$launcher" 2>/dev/null || status=$?
  {
    echo "emulator=$emulator system=$system rom=$rom"
    echo "alive_until_the_end=$alive launcher_status=$status"
    echo "listening=$(grep -c 'semu: listening for keys' "$out/$label.log" || true)"
    echo "bezel_actions=$(grep -c 'semu: action visual.bezel.next (keyboard)' "$out/$label.log" || true) shader_actions=$(grep -c 'semu: action visual.shader.next (keyboard)' "$out/$label.log" || true)"
    echo "variants_file=$(head -c 300 "$root/state/$emulator/semu-render-variants.env" 2>/dev/null | head -7 | tr '\n' ' ')"
    echo "journal_records(action slot)=$(od -A n -t d4 -w56 -v "$journal" 2>/dev/null | while read -r -a words; do printf '%s %s; ' "${words[6]}" "${words[8]}"; done)"  # 32-bit words 6 and 8 of each 56-byte record
    echo "switches=$(grep -o 'phase=switch.*' "$root/state/$emulator/semu-render-evidence.log" 2>/dev/null | grep -o 'bezel_art=[^ ]*\|shader_preset=[^ ]*\|layout=[^ ]*\|bezel_index=[^ ]*\|shader_index=[^ ]*\|reload_ms=[^ ]*\|frame_ms=[^ ]*' | tr '\n' ' ')"
    echo "renderer_switch_lines=$(grep -c 'semu-renderer: switched to' "$out/$label.log" || true)"
    echo "saved=$("$jq" -c '.visual' "$root/home/semu/semu.json" 2>/dev/null || echo none)"
  } > "$out/$label.result"
  cat "$out/$label.result"
done < "$out/cases"
kill "$xvfb_pid" 2>/dev/null || true
cat "$out"/*.result > "$out/result"
echo "live-switch: done; judge the captures in $out"
