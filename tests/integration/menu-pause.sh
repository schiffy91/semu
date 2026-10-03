#!/usr/bin/env bash
# Does a paused standalone emulator still present? Opening the Semu menu pauses the emulator only
# where it keeps drawing while paused (emulator.json menu.pause), because the compositor draws the
# menu at the emulator's own present: a paused emulator that stops presenting would hide it.
# On Linux in the podman VM under a private Xvfb with openbox (software GL and Vulkan), each
# case launches a real game through `semu launch`, pauses it with the emulator's own pause key
# (XTest, the way Steam types into Game Mode), appends a menu-toggle record to the action journal
# and captures; then it resumes, captures again, and presses Semu's save chord, listing the
# files it wrote.
# A menu that shows while paused means the emulator presents while paused. PCSX2 and Dolphin map
# guest memory through /dev/shm, hence --shm-size. ROMs and the PS2 BIOS folder are mounted
# read-only, scratch lives in mktemp -d, the container is left exited and nothing is removed.
# Never touches the Mac display.
#
#   menu-pause.sh OUT_DIR CASE...    # on the Mac; CASE is emulator:system:pause-key:rom-path
#   menu-pause.sh --inside OUT_DIR   # in Linux, reading OUT_DIR/cases
#
# SEMU_REV=<rev> builds that commit instead of this checkout's tracked files; SEMU_PS2_BIOS=DIR
# mounts a PS2 BIOS folder; WAIT seconds before pausing (default 120). Each case writes
# OUT/<n>-<emulator>.result (moving flags, menu pixel counts) and captures to judge by eye.
set -eu
image=docker.io/nixos/nix:latest
if [ "${1:-}" != "--inside" ]; then
  out="$(mkdir -p "${1:?usage: menu-pause.sh OUT_DIR CASE...}" && cd "$1" && pwd -P)"
  shift
  repository="$(cd "$(dirname "$0")/../.." && pwd -P)"
  [ "$(podman machine inspect --format '{{.State}}')" = running ] || podman machine start
  mounts=()
  : > "$out/cases"
  for case in "$@"; do
    emulator="${case%%:*}"; rest="${case#*:}"; system="${rest%%:*}"; rest="${rest#*:}"; key="${rest%%:*}"; rom="${rest#*:}"
    printf '%s\t%s\t%s\t%s\n' "$emulator" "$system" "$key" "$(basename "$rom")" >> "$out/cases"
    mounts+=(-v "$rom:/roms/$system/$(basename "$rom"):ro")
  done
  [ -n "${SEMU_PS2_BIOS:-}" ] && mounts+=(-v "$SEMU_PS2_BIOS:/emulation/PCSX2/config/bios:ro")
  name="semu-menu-pause-$(date +%Y%m%d%H%M%S)"  # left behind exited
  echo "container $name, results in $out"
  exec podman run --name "$name" --platform linux/amd64 --privileged --shm-size=4g -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix \
    -v "$repository":/src:ro -v "$out":/out "${mounts[@]}" -e SEMU_REV="${SEMU_REV:-}" -e WAIT="${WAIT:-120}" \
    -e NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0" "$image" bash /src/tests/integration/menu-pause.sh --inside /out
fi
out="$2"
git config --global --add safe.directory '*'
source="git+file:///src${SEMU_REV:+?rev=$SEMU_REV}"
package() { nix build --no-link --inputs-from /src "nixpkgs#$1" >>"$out/nix.log" 2>&1 && nix eval --raw --inputs-from /src "nixpkgs#$1.outPath"; }
echo "building $source (log: $out/nix.log)"
bundle="$(nix build --no-link --print-out-paths "$source#packages.x86_64-linux.semu" 2>>"$out/nix.log" | tail -1)"
mesa="$(package mesa)"; xvfb="$(package xvfb)"; xdotool="$(package xdotool)"; xwd="$(package xwd)"; magick="$(package imagemagick)/bin/magick"; find="$(package findutils)/bin/find"; openbox="$(package openbox)"
[ -x "$bundle/bin/semu" ] || { echo "FAIL: no bundle" | tee "$out/result"; exit 1; }
echo "bundle $bundle" | tee "$out/bundle"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe LIBGL_DRIVERS_PATH="$mesa/lib/dri" __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_DIRS="$mesa/share/glvnd/egl_vendor.d" LD_LIBRARY_PATH="$mesa/lib" VK_DRIVER_FILES="$(ls "$mesa"/share/vulkan/icd.d/lvp_icd*.json | head -1)"
printf "pcm.!default {\n  type null\n}\nctl.!default {\n  type hw\n  card 0\n}\n" > "$out/alsa-null.conf"  # cubeb (PCSX2) falls back to ALSA: a silent device, or a modal error hides the game
export SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy QT_QPA_PLATFORM=xcb ALSA_CONFIG_PATH="$out/alsa-null.conf" PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none
unset WAYLAND_DISPLAY
display=:93
width=1280; height=800
"$xvfb/bin/Xvfb" "$display" -screen 0 ${width}x${height}x24 >"$out/xvfb.log" 2>&1 & xvfb_pid=$!
sleep 2
DISPLAY="$display" "$openbox/bin/openbox" --sm-disable >"$out/openbox.log" 2>&1 &  # a window manager gives the game window the keyboard focus Qt checks, as gamescope does
sleep 2
x() { DISPLAY="$display" "$xdotool/bin/xdotool" "$@"; }
shot() { DISPLAY="$display" "$xwd/bin/xwd" -root -silent | "$magick" xwd:- "$1"; }
differs() { [ "$("$magick" compare -metric AE "$1" "$2" null: 2>&1 | cut -d' ' -f1)" != 0 ]; }
menu_pixels() { "$magick" "$1" -fuzz 3% -fill white -opaque 'rgb(18,112,120)' -fill black +opaque white -format '%[fx:round(mean*w*h)]' info:; }  # the menu header's teal
record() {  # one 56-byte menu-toggle record (abi 1, sequence 1000) appended to the journal
  printf '\001\000\000\000\070\000\000\000\350\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000semu\000\000\000\000\000\000\000\000\000\000\000\000' >> "$1"
}
focus() {  # size every named window to the screen, then activate the last one, the game
  window=""
  for window in $(x search --onlyvisible --name . 2>/dev/null); do x windowmove "$window" 0 0 windowsize "$window" "$width" "$height" 2>/dev/null || true; done
  sleep 2
  x mousemove $((width / 2)) $((height / 2))
  [ -n "$window" ] && { x windowactivate --sync "$window" 2>/dev/null || true; x windowfocus --sync "$window" 2>/dev/null || true; }
  echo "focus: $(x getwindowfocus getwindowname 2>/dev/null)" >> "$out/focus.log"
}
press() { x key --delay 80 "$1"; }

number=0
while IFS="$(printf '\t')" read -r emulator system key rom; do
  number=$((number + 1))
  label="$number-$emulator"
  root="$(mktemp -d)"
  mkdir -p "$root/home" "$root/emulation"
  settings="{\"paths\":{\"roms\":\"/roms\",\"state_root\":\"$root/state\",\"content_root\":\"$root/content\",\"emulation_root\":\"/emulation\",\"bios\":\"$root/emulation\"}}"
  HOME="$root/home" DISPLAY="$display" SEMU_RENDER_DEBUG=1 "$bundle/bin/semu" launch "$emulator" --system "$system" --rom "$rom" \
    --settings-json "$settings" --semu-home "$root/home/semu" > "$out/$label.log" 2>&1 &
  launcher=$!
  sleep "${WAIT:-120}"
  focus
  sleep 3
  shot "$out/$label-1-running-a.png"; sleep 1; shot "$out/$label-1-running-b.png"
  press "$key"; sleep 4
  shot "$out/$label-2-paused-a.png"; sleep 1; shot "$out/$label-2-paused-b.png"
  record "$root/state/$emulator/semu-render-actions.bin"; sleep 5
  shot "$out/$label-3-menu-while-paused.png"
  press "$key"; sleep 5
  shot "$out/$label-4-menu-after-resume.png"
  touch "$root/before-save"; press ctrl+s; sleep 8  # Semu's save chord, typed the way the radial sends it
  saved=""
  while IFS= read -r file; do
    case "$file" in *.log|*.txt|*.ini|*.cfg|*.json|*.toml|*.bin) ;; *) saved="$saved ${file#"$root"/}" ;; esac
  done < <("$find" "$root/state" "$root/content" -type f -newer "$root/before-save" 2>/dev/null)
  alive=yes; kill -0 "$launcher" 2>/dev/null || alive=no
  kill -TERM "$launcher" 2>/dev/null || true
  status=0; wait "$launcher" 2>/dev/null || status=$?
  differs "$out/$label-1-running-a.png" "$out/$label-1-running-b.png" && running=yes || running=no
  differs "$out/$label-2-paused-a.png" "$out/$label-2-paused-b.png" && paused_moving=yes || paused_moving=no
  {
    echo "emulator=$emulator system=$system key=$key rom=$rom"
    echo "alive_until_the_end=$alive launcher_status=$status"
    echo "moving_before_pause=$running"
    echo "moving_after_pause=$paused_moving"
    echo "menu_pixels_while_paused=$(menu_pixels "$out/$label-3-menu-while-paused.png")"
    echo "menu_pixels_after_resume=$(menu_pixels "$out/$label-4-menu-after-resume.png")"
    echo "journal_bytes=$(wc -c < "$root/state/$emulator/semu-render-actions.bin" 2>/dev/null || echo 0)"
    echo "files_after_ctrl_s=$saved"
  } > "$out/$label.result"
  cat "$out/$label.result"
done < "$out/cases"
kill "$xvfb_pid" 2>/dev/null || true
cat "$out"/*.result > "$out/result"
echo "menu-pause: done; judge the captures in $out"
