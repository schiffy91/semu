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
# mounts a PS2 BIOS folder; SEMU_BIOS=DIR mounts a firmware folder read-only as paths.bios (the PS1 BIOS for
# Beetle PSX); SIZE=WxH sizes the screen (default 1280x800, the Deck); CAPTURE_FRAME=N has the renderer save its Nth composed frame,
# the emulator's own overlay included (OUT/<n>-<emulator>-<system>-semu-render-final.ppm, beside the launch's receipts and PCSX2's
# emulog); WAIT seconds before the first chord (default 120); SCALE=2x starts the system at that render scale (the
# result's render_scales and scale_config lines: the receipts' native and scaled sizes, the emulator's own key); PLACEMENT=bezel
# shows a handheld's whole shell instead of the cropped integer picture; CHORDS="ctrl+shift+f ctrl+shift+r,ctrl+shift+r"
# presses more radial chords after the menu, each with its toast and picture (a,b: twice, a second apart). Each case writes
# OUT/<n>-<emulator>.result (actions seen, journal records, switch receipts, saved choices) and
# captures to judge by eye: three in the two seconds after each chord, as software GL draws slowly.
# PADS=N first plugs N replicas of Steam's virtual pad (slots 0..N-1, named by a Steam slot file: the
# Deck, then Xbox Series X pads) so the launch deals players; BASICS=0 skips the bezel, shader and menu
# presses; for Dolphin the result also lists each layout switch, the players dealt, every open of the
# Wiimote profile folder's files (inotify: Dolphin reading the profile a press loaded) and the Wii Remote
# sources the last Dolphin.ini holds.
set -eu
image=docker.io/nixos/nix:latest
if [ "${1:-}" != "--inside" ] && [ "${1:-}" != "--build" ]; then
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
  [ -n "${SEMU_BIOS:-}" ] && mounts+=(-v "$SEMU_BIOS:/bios:ro")
  name="semu-live-switch-$(date +%Y%m%d%H%M%S)"  # left behind exited
  echo "container $name, results in $out"
  environment=(-e SEMU_REV="${SEMU_REV:-}" -e WAIT="${WAIT:-120}" -e PLACEMENT="${PLACEMENT:-}" -e SCALE="${SCALE:-}" -e CHORDS="${CHORDS:-}" -e SETTLE="${SETTLE:-}" -e PADS="${PADS:-0}" -e BASICS="${BASICS:-1}" -e SIZE="${SIZE:-1280x800}" -e CAPTURE_FRAME="${CAPTURE_FRAME:-}" -e FIRMWARE="${SEMU_BIOS:+/bios}")
  nixConfig="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0"
  if [ "${PADS:-0}" -gt 0 ]; then  # only real root may open the VM's /dev/uinput: build rootless, then run rootful on that store, read-only, with /dev/input bound
    podman run --name "$name-build" --platform linux/amd64 --privileged -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix \
      -v "$repository":/src:ro -v "$out":/out "${environment[@]}" -e NIX_CONFIG="$nixConfig" "$image" bash /src/tests/integration/live-switch.sh --build /out
    store="$(podman volume inspect semu-nix-x86 --format '{{.Mountpoint}}')"
    podman machine ssh "sudo podman image exists $image" || podman image save "$image" | podman machine ssh 'sudo podman image load'
    quoted=""; for mount in "${mounts[@]}"; do quoted="$quoted '$mount'"; done
    quotedEnvironment=""; for entry in "${environment[@]}"; do quotedEnvironment="$quotedEnvironment '$entry'"; done
    exec podman machine ssh "sudo podman run --name $name-run --platform linux/amd64 --privileged --security-opt label=disable --shm-size=4g \
      -v /dev/input:/dev/input -v '$store':/nix:ro -v '$repository':/src:ro -v '$out':/out $quoted $quotedEnvironment $image bash /src/tests/integration/live-switch.sh --inside /out"
  fi
  exec podman run --name "$name" --platform linux/amd64 --privileged --shm-size=4g -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix \
    -v "$repository":/src:ro -v "$out":/out "${mounts[@]}" "${environment[@]}" -e NIX_CONFIG="$nixConfig" "$image" bash /src/tests/integration/live-switch.sh --inside /out
fi
out="$2"
if [ "$1" = "--build" ] || [ ! -f "$out/paths.env" ]; then  # the bundle and the tools; a rootful run reads them from paths.env
  git config --global --add safe.directory '*'
  source="git+file:///src${SEMU_REV:+?rev=$SEMU_REV}"
  package() { nix build --no-link --inputs-from /src "nixpkgs#$1" >>"$out/nix.log" 2>&1 && nix eval --raw --inputs-from /src "nixpkgs#$1.outPath"; }
  echo "building $source (log: $out/nix.log)"
  bundle="$(nix build --no-link --print-out-paths "$source#packages.x86_64-linux.semu" 2>>"$out/nix.log" | tail -1)"
  mesa="$(package mesa)"; xvfb="$(package xvfb)"; xdotool="$(package xdotool)"; xwd="$(package xwd)"; magick="$(package imagemagick)/bin/magick"; openbox="$(package openbox)"; jq="$(package jq)/bin/jq"
  inotify="$(package inotify-tools)"
  pad=""
  if [ "${PADS:-0}" -gt 0 ]; then  # Steam's virtual pads, one per player
    btrcpy="$(nix build --no-link --print-out-paths "$source#packages.x86_64-linux.btrcpy" 2>>"$out/nix.log" | tail -1)"; gcc="$(package gcc)"
    "$btrcpy/bin/btrcpy" --strict-imports --no-cache --no-stdlib /src/tests/visual/virtual_pad.btrc -o "$out/virtual_pad.c" >/dev/null
    "$gcc/bin/gcc" -std=c11 -O1 -w -I/src/src/launch "$out/virtual_pad.c" -o "$out/virtual_pad" && pad="$out/virtual_pad"
  fi
  printf 'bundle=%s\nmesa=%s\nxvfb=%s\nxdotool=%s\nxwd=%s\nmagick=%s\nopenbox=%s\njq=%s\ninotify=%s\npad=%s\n' "$bundle" "$mesa" "$xvfb" "$xdotool" "$xwd" "$magick" "$openbox" "$jq" "$inotify" "$pad" > "$out/paths.env"
  [ "$1" = "--build" ] && exit 0
fi
. "$out/paths.env"
if [ "${PADS:-0}" -gt 0 ]; then export SDL_JOYSTICK_DISABLE_UDEV=1; fi  # no udev daemon in the container: SDL watches /dev/input itself
[ -x "$bundle/bin/semu" ] || { echo "FAIL: no bundle" | tee "$out/result"; exit 1; }
echo "bundle $bundle" | tee "$out/bundle"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe LIBGL_DRIVERS_PATH="$mesa/lib/dri" __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_DIRS="$mesa/share/glvnd/egl_vendor.d" LD_LIBRARY_PATH="$mesa/lib" VK_DRIVER_FILES="$(ls "$mesa"/share/vulkan/icd.d/lvp_icd*.json | head -1)"
printf "pcm.!default {\n  type null\n}\nctl.!default {\n  type hw\n  card 0\n}\n" > "$out/alsa-null.conf"  # cubeb (PCSX2) falls back to ALSA: a silent device, or a modal error hides the game
export SDL_AUDIODRIVER=dummy SDL_AUDIO_DRIVER=dummy QT_QPA_PLATFORM=xcb ALSA_CONFIG_PATH="$out/alsa-null.conf" PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=semu-none
unset WAYLAND_DISPLAY
display=:92
size="${SIZE:-1280x800}"; width="${size%x*}"; height="${size#*x}"
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
  own="${PLACEMENT:+\"placement\":\"$PLACEMENT\",}${SCALE:+\"render_scale\":\"$SCALE\",}"  # the system's own choices: SCALE=2x starts at that render scale
  visual=""; [ -n "$own" ] && visual=",\"visual\":{\"systems\":{\"$system\":{${own%,}}}}"
  settings="{\"paths\":{\"roms\":\"/roms\",\"state_root\":\"$root/state\",\"content_root\":\"$root/content\",\"emulation_root\":\"/emulation\",\"bios\":\"${FIRMWARE:-$root/emulation}\"}$visual}"
  pads=()
  if [ -n "$pad" ]; then
    names=("Steam Deck Controller" "Xbox Series X Controller" "Xbox Series X Controller" "DualSense Wireless Controller")
    : > "$root/steam-slots"
    for slot in $(seq 0 $((PADS - 1))); do
      printf "[slot %s]\nVID=0x28de\nPID=0x11ff\nname=%s\n" "$slot" "${names[$slot]}" >> "$root/steam-slots"
      "$pad" --steam-virtual-pad-slot "$slot" 900 sleep:1 > "$out/$label-pad$slot.log" 2>&1 & pads+=($!)
    done
    export SteamVirtualGamepadInfo="$root/steam-slots"
    sleep 2
  fi
  HOME="$root/home" DISPLAY="$display" SEMU_RENDER_DEBUG=1 SEMU_RENDER_CAPTURE_FRAME="${CAPTURE_FRAME:-}" "$bundle/bin/semu" launch "$emulator" --system "$system" --rom "$rom" \
    --settings-json "$settings" --semu-home "$root/home/semu" > "$out/$label.log" 2>&1 &
  launcher=$!
  profiles="$root/state/$emulator/dolphin-user/Config/Profiles"
  ( for second in $(seq 1 60); do [ -d "$profiles" ] && break; sleep 1; done; exec "$inotify/bin/inotifywait" -m -r -e open --timefmt %T --format "%T %w%f %e" "$profiles" ) > "$out/$label.profile-opens" 2>&1 & watcher=$!
  sleep "${WAIT:-120}"
  focus
  sleep 3
  shot "$out/$label-1-before.png"
  if [ "${BASICS:-1}" != 0 ]; then
    press ctrl+shift+b; burst "$out/$label-2-bezel"  # the toast, while the new bezel decodes
    sleep 4; shot "$out/$label-3-bezel-after.png"
    press ctrl+shift+v; burst "$out/$label-4-shader"  # LOADING, then the plain toast once the chain is built
    sleep 6; shot "$out/$label-5-shader-after.png"
  fi
  press ctrl+m; sleep 3
  shot "$out/$label-6-menu.png"
  press ctrl+m; sleep 2
  step=7
  for chord in ${CHORDS:-}; do  # the radial's newer slots, e.g. ctrl+shift+f (Fit) and ctrl+shift+r twice (Reset): the toast, then the picture
    for key in ${chord//,/ }; do press "$key"; [ "$key" = "${chord##*,}" ] || sleep 1; done  # a,b: pressed a second apart, inside a confirm window
    burst "$out/$label-$step-${chord//,/-}"
    sleep 3; shot "$out/$label-$step-${chord//,/-}-after.png"
    step=$((step + 1))
  done
  if [ -n "${SETTLE:-}" ]; then sleep "$SETTLE"; shot "$out/$label-final.png"; fi  # SETTLE seconds later, e.g. the game Aspect restarted on its new output
  journal="$root/state/$emulator/semu-render-actions.bin"
  leaf=""  # the running emulator's argv (after an Aspect restart, the second one's)
  for process in /proc/[0-9]*; do
    line="$(tr '\0' ' ' < "$process/cmdline" 2>/dev/null || true)"
    case "$line" in *"$rom"*) case "$line" in *semu\ launch*) ;; *) leaf="$line" ;; esac ;; esac
  done
  alive=yes; kill -0 "$launcher" 2>/dev/null || alive=no
  kill -TERM "$launcher" 2>/dev/null || true
  status=0; wait "$launcher" 2>/dev/null || status=$?
  kill "$watcher" 2>/dev/null || true
  for pid in "${pads[@]}"; do kill "$pid" 2>/dev/null || true; done
  for evidence in semu-render-final.ppm semu-render-evidence.log; do  # the frame-CAPTURE_FRAME picture (the emulator's own overlay included) and every receipt
    if [ -f "$root/state/$emulator/$evidence" ]; then cp "$root/state/$emulator/$evidence" "$out/$label-$evidence"; fi
  done
  find "$root/state/$emulator" -name emulog.txt -exec cp {} "$out/$label-emulog.txt" \; 2>/dev/null || true  # PCSX2's own log
  {
    echo "emulator=$emulator system=$system rom=$rom"
    echo "alive_until_the_end=$alive launcher_status=$status"
    echo "listening=$(grep -c 'semu: listening for keys' "$out/$label.log" || true)"
    echo "bezel_actions=$(grep -c 'semu: action visual.bezel.next (keyboard)' "$out/$label.log" || true) shader_actions=$(grep -c 'semu: action visual.shader.next (keyboard)' "$out/$label.log" || true)"
    echo "variants_file=$(head -c 300 "$root/state/$emulator/semu-render-variants.env" 2>/dev/null | head -7 | tr '\n' ' ')"
    echo "journal_records(action slot reserved)=$(od -A n -t d4 -w56 -v "$journal" 2>/dev/null | while read -r -a words; do printf '%s %s %s; ' "${words[6]}" "${words[8]}" "${words[9]}"; done)"  # 32-bit words 6, 8 and 9 of each 56-byte record
    echo "leaf_argv=$leaf"
    echo "restarts=$(grep -a -c 'semu: restarting' "$out/$label.log" || true) aspect_actions=$(grep -c 'semu: action visual.output.next' "$out/$label.log" || true)"
    echo "placements=$(grep -c 'semu-renderer: placement' "$out/$label.log" || true) reset_actions=$(grep -c 'semu: action system.reset' "$out/$label.log" || true) fit_actions=$(grep -c 'semu: action visual.placement.next' "$out/$label.log" || true)"
    echo "switches=$(grep -o 'phase=switch.*' "$root/state/$emulator/semu-render-evidence.log" 2>/dev/null | grep -o 'bezel_art=[^ ]*\|shader_preset=[^ ]*\|layout=[^ ]*\|bezel_index=[^ ]*\|shader_index=[^ ]*\|reload_ms=[^ ]*\|frame_ms=[^ ]*' | tr '\n' ' ')"
    echo "renderer_switch_lines=$(grep -c 'semu-renderer: switched to' "$out/$label.log" || true)"
    echo "saved=$("$jq" -c '.visual' "$root/home/semu/semu.json" 2>/dev/null || echo none)"
    echo "players=$(grep -a -o "semu: player .*" "$out/$label.log" | cut -c14- | tr "\n" ";")"
    echo "layout_switches=$(grep -a -E "semu: P[0-9] layout" "$out/$label.log" | tr "\n" ";")"
    echo "layout_actions=$(grep -a -c "semu: action controller.layout.next" "$out/$label.log" || true) players_actions=$(grep -a -c "semu: action ui.players" "$out/$label.log" || true)"
    echo "profile_opens=$(grep -c "Semu.ini OPEN" "$out/$label.profile-opens" 2>/dev/null || true) ($(grep "Semu.ini OPEN" "$out/$label.profile-opens" 2>/dev/null | cut -d" " -f1 | tr "\n" " "))"
    echo "remote_sources=$(grep -E "^(WiimoteSource|SIDevice)" "$root/state/$emulator/dolphin-user/Config/Dolphin.ini" 2>/dev/null | tr "\n" " ")"
    echo "remote2_device=$(grep -A 1 "^\[Wiimote2\]" "$root/state/$emulator/dolphin-user/Config/WiimoteNew.ini" 2>/dev/null | grep "^Device" | head -1)"
    echo "saved_input=$("$jq" -c ".input" "$root/home/semu/semu.json" 2>/dev/null || echo none)"
    echo "unsafe_settings_notices=$(grep -a -c "Unsafe Settings" "$out/$label-emulog.txt" 2>/dev/null || true) sources=$(grep -a -o "surface0_source=[^ ]* surface0_native=[^ ]*\|surface0_native=[^ ]* surface0_source=[^ ]*" "$out/$label-semu-render-evidence.log" 2>/dev/null | sort | uniq -c | tr -s " \n" " ;")"
    echo "render_scales=$(grep -a -o "render_scale=[0-9]* frames_scaled=[0-9]*\|surface0_native=[^ ]*\|surface0_scaled=[^ ]*" "$out/$label-semu-render-evidence.log" 2>/dev/null | sort | uniq -c | tr -s " \n" " ;") debug_lanes=$(grep -a -o "native [0-9]*x[0-9]* scaled [0-9]*x[0-9]* source [0-9]*x[0-9]*" "$out/$label.log" | sort | uniq -c | tr -s " \n" " ;")"
    echo "scale_config=$(find "$root/state/$emulator" \( -name retroarch-core-options.cfg -o -name PCSX2.ini -o -name GFX.ini -o -name ppsspp.ini -o -name emu.cfg \) -exec grep -a -h -E "43screensize|EnableNativeResFactor|internal_resolution|resolution_factor|^upscale_multiplier|^InternalResolution|^rend.Resolution" {} + 2>/dev/null | tr "\n" " ") scale_actions=$(grep -c 'semu: action visual.scale.next' "$out/$label.log" || true)"
  } > "$out/$label.result"
  cat "$out/$label.result"
done < "$out/cases"
kill "$xvfb_pid" 2>/dev/null || true
cat "$out"/*.result > "$out/result"
echo "live-switch: done; judge the captures in $out"
