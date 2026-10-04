#!/usr/bin/env bash
# The mods library end to end in a real emulator: each case launches a game through `semu launch` on Linux in
# the podman VM (private Xvfb, openbox, software GL and Vulkan) with paths.mods at a library mounted
# READ-ONLY, so a write into it would fail. It records what Semu linked and wrote (the plan's mods lines, the
# links, the manifest, the per-title files and update choices) and what the emulator logged about them after
# WAIT seconds, and captures the screen. ROMs, keys and the library are mounted read-only, scratch lives in
# mktemp -d, the container is left exited and nothing is removed. Never touches the Mac display.
#
#   mods.sh OUT_DIR CASE...    # on the Mac; CASE is emulator:system:rom-path
#   mods.sh --inside OUT_DIR   # in Linux, reading OUT_DIR/cases
#
# SEMU_MODS_MOUNTS, SEMU_ROM_MOUNTS and SEMU_EMULATION_MOUNTS are "SOURCE=DEST;..." lists: each SOURCE is
# mounted read-only at /library/DEST, /roms/DEST or /emulation/DEST (e.g. an old Azahar load/mods folder at
# azahar/mods, an updates folder at switch/updates, keys at Ryujinx/config/system). SEMU_REV=<rev> builds that
# commit instead of this checkout's tracked files; WAIT seconds before the capture (default 150).
set -eu
image=docker.io/nixos/nix:latest
if [ "${1:-}" != "--inside" ]; then
  out="$(mkdir -p "${1:?usage: mods.sh OUT_DIR CASE...}" && cd "$1" && pwd -P)"
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
  for pair in "library:${SEMU_MODS_MOUNTS:-}" "roms:${SEMU_ROM_MOUNTS:-}" "emulation:${SEMU_EMULATION_MOUNTS:-}"; do
    base="${pair%%:*}"; list="${pair#*:}"
    while [ -n "$list" ]; do
      entry="${list%%;*}"; [ "$entry" = "$list" ] && list="" || list="${list#*;}"
      [ -n "$entry" ] && mounts+=(-v "${entry%%=*}:/$base/${entry#*=}:ro")
    done
  done
  name="semu-mods-$(date +%Y%m%d%H%M%S)"  # left behind exited
  echo "container $name, results in $out"
  exec podman run --name "$name" --platform linux/amd64 --privileged --shm-size=4g -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix \
    -v "$repository":/src:ro -v "$out":/out "${mounts[@]}" -e SEMU_REV="${SEMU_REV:-}" -e WAIT="${WAIT:-150}" \
    -e NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0" "$image" bash /src/tests/integration/mods.sh --inside /out
fi
out="$2"
git config --global --add safe.directory '*'
source="git+file:///src${SEMU_REV:+?rev=$SEMU_REV}"
package() { nix build --no-link --inputs-from /src "nixpkgs#$1" >>"$out/nix.log" 2>&1 && nix eval --raw --inputs-from /src "nixpkgs#$1.outPath"; }
echo "building $source (log: $out/nix.log)"
bundle="$(nix build --no-link --print-out-paths "$source#packages.x86_64-linux.semu" 2>>"$out/nix.log" | tail -1)"
mesa="$(package mesa)"; xvfb="$(package xvfb)"; xwd="$(package xwd)"; magick="$(package imagemagick)/bin/magick"; openbox="$(package openbox)"; jq="$(package jq)/bin/jq"
[ -x "$bundle/bin/semu" ] || { echo "FAIL: no bundle" | tee "$out/result"; exit 1; }
echo "bundle $bundle" | tee "$out/bundle"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe LIBGL_DRIVERS_PATH="$mesa/lib/dri" __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_DIRS="$mesa/share/glvnd/egl_vendor.d" LD_LIBRARY_PATH="$mesa/lib" VK_DRIVER_FILES="$(ls "$mesa"/share/vulkan/icd.d/lvp_icd*.json | head -1)"
export SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy QT_QPA_PLATFORM=xcb PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none
unset WAYLAND_DISPLAY
display=:93
"$xvfb/bin/Xvfb" "$display" -screen 0 1280x800x24 >"$out/xvfb.log" 2>&1 & xvfb_pid=$!
sleep 2
DISPLAY="$display" "$openbox/bin/openbox" --sm-disable >"$out/openbox.log" 2>&1 &
sleep 2
indent() { while IFS= read -r line; do printf "%s%s\n" "$1" "$line"; done; }  # the image has no sed
mkdir -p /library
find /library > "$out/library-before.txt" 2>/dev/null || true
number=0
while IFS="$(printf '\t')" read -r emulator system rom; do
  number=$((number + 1))
  label="$number-$emulator-$system"
  root="$(mktemp -d)"
  mkdir -p "$root/home" "$root/bios"
  settings="{\"paths\":{\"roms\":\"/roms\",\"state_root\":\"$root/state\",\"content_root\":\"$root/content\",\"emulation_root\":\"/emulation\",\"bios\":\"$root/bios\",\"mods\":\"/library\"},\"visual\":{\"display\":{\"width\":1280,\"height\":800}}}"
  HOME="$root/home" "$bundle/bin/semu" launch "$emulator" --system "$system" --rom "$rom" --settings-json "$settings" --semu-home "$root/home/semu" --print-plan > "$out/$label.plan.json" 2>&1 || true
  HOME="$root/home" DISPLAY="$display" SEMU_RENDER_DEBUG=1 "$bundle/bin/semu" launch "$emulator" --system "$system" --rom "$rom" \
    --settings-json "$settings" --semu-home "$root/home/semu" > "$out/$label.log" 2>&1 &
  launcher=$!
  sleep "${WAIT:-150}"
  DISPLAY="$display" "$xwd/bin/xwd" -root -silent | "$magick" xwd:- "$out/$label.png" || true
  kill -TERM "$launcher" 2>/dev/null || true
  wait "$launcher" 2>/dev/null || true
  state="$root/state/$emulator"
  {
    echo "emulator=$emulator system=$system rom=$rom"
    echo "plan_mods=$("$jq" -c '.mods' "$out/$label.plan.json" 2>/dev/null || echo none) notice=$("$jq" -c '.notice' "$out/$label.plan.json" 2>/dev/null)"
    echo "links:"; find "$state" -type l -printf '  %p -> %l\n' 2>/dev/null | grep -v '/lib/\|/nix/' || true
    echo "manifest:"; indent "  " < "$state/semu-mods.tsv" 2>/dev/null || true
    echo "title_files:"; for file in "$state"/config/*/custom/*.ini "$state"/config/Ryujinx/games/*/updates.json; do [ -f "$file" ] && { echo "  $file"; indent "    " < "$file"; } || true; done
    echo "emulator_log:"; cat "$state"/data/*/log/*.txt "$state"/config/Ryujinx/Logs/*.log "$out/$label.log" 2>/dev/null \
      | grep -E 'patching code.bin|per application config|custom_tex|Custom|textures|Found (enabled|disabled) mod|NSO .* replaced|npdm replaced|update|Update' | sort -u | head -40 | indent "  "
  } > "$out/$label.result"
  cat "$out/$label.result"
done < "$out/cases"
find /library > "$out/library-after.txt" 2>/dev/null || true
if cmp -s "$out/library-before.txt" "$out/library-after.txt"; then echo "library: unchanged ($(wc -l < "$out/library-after.txt") entries)" | tee "$out/library.result"; else echo "library: CHANGED" | tee "$out/library.result"; fi
kill "$xvfb_pid" 2>/dev/null || true
cat "$out"/*.result > "$out/result"
echo "mods: done; read $out/result and judge the captures"
