#!/usr/bin/env bash
# Audio end to end: each case launches a game through `semu launch` on Linux in the podman VM (private Xvfb,
# openbox, software GL and Vulkan) beside a PulseAudio server whose only sink is a null sink, found the way the
# Deck's PipeWire pulse server is found ($XDG_RUNTIME_DIR/pulse/native, no PULSE_SERVER). Every POLL seconds for
# WAIT seconds it lists the server's sink inputs and the null sink's loudest sample over one second (sox), so a
# case passes when the emulator opened an output stream, and the peak shows the game playing into it.
# A case with CONTROL after the ROM runs the same plan's argv and environment once more without Semu, with the
# Audio block taken out of Cemu's settings.xml: the stream must then be missing. ROMs and keys are mounted
# read-only, scratch lives in mktemp -d, the container is left exited and nothing is removed. Never touches the
# Mac display.
#
#   audio.sh OUT_DIR CASE...    # on the Mac; CASE is emulator:system:rom-path[:control]
#   audio.sh --inside OUT_DIR   # in Linux, reading OUT_DIR/cases
#
# SEMU_EMULATION_MOUNTS is a "SOURCE=DEST;..." list mounted read-only at /emulation/DEST (Cemu's keys at
# Cemu/data/keys.txt). SEMU_REV=<rev> builds that commit instead of this checkout's tracked files.
set -eu
image=docker.io/nixos/nix:latest
if [ "${1:-}" != "--inside" ]; then
  out="$(mkdir -p "${1:?usage: audio.sh OUT_DIR CASE...}" && cd "$1" && pwd -P)"
  shift
  repository="$(cd "$(dirname "$0")/../.." && pwd -P)"
  [ "$(podman machine inspect --format '{{.State}}')" = running ] || podman machine start
  mounts=()
  : > "$out/cases"
  for case in "$@"; do
    emulator="${case%%:*}"; rest="${case#*:}"; system="${rest%%:*}"; rom="${rest#*:}"; control=""
    case "$rom" in *:control) rom="${rom%:control}"; control=control ;; esac
    printf '%s\t%s\t%s\t%s\n' "$emulator" "$system" "$(basename "$rom")" "$control" >> "$out/cases"
    mounts+=(-v "$rom:/roms/$system/$(basename "$rom"):ro")
  done
  list="${SEMU_EMULATION_MOUNTS:-}"
  while [ -n "$list" ]; do
    entry="${list%%;*}"; [ "$entry" = "$list" ] && list="" || list="${list#*;}"
    [ -n "$entry" ] && mounts+=(-v "${entry%%=*}:/emulation/${entry#*=}:ro")
  done
  name="semu-audio-$(date +%Y%m%d%H%M%S)"  # left behind exited
  echo "container $name, results in $out"
  exec podman run --name "$name" --platform linux/amd64 --privileged --shm-size=4g -v semu-nix-x86:/nix -v semu-nix-cache:/root/.cache/nix \
    -v "$repository":/src:ro -v "$out":/out "${mounts[@]}" -e SEMU_REV="${SEMU_REV:-}" -e WAIT="${WAIT:-180}" -e POLL="${POLL:-5}" \
    -e NIX_CONFIG="experimental-features = nix-command flakes
filter-syscalls = false
sandbox = false
max-jobs = 4
cores = 0" "$image" bash /src/tests/integration/audio.sh --inside /out
fi
out="$2"
git config --global --add safe.directory '*'
source="git+file:///src${SEMU_REV:+?rev=$SEMU_REV}"
package() { nix build --no-link --inputs-from /src "nixpkgs#$1" >>"$out/nix.log" 2>&1 && nix eval --raw --inputs-from /src "nixpkgs#$1.outPath"; }
echo "building $source (log: $out/nix.log)"
bundle="$(nix build --no-link --print-out-paths "$source#packages.x86_64-linux.semu" 2>>"$out/nix.log" | tail -1)"
mesa="$(package mesa)"; xvfb="$(package xvfb)"; xwd="$(package xwd)"; magick="$(package imagemagick)/bin/magick"; openbox="$(package openbox)"
jq="$(package jq)/bin/jq"; pulse="$(package pulseaudio)"; sox="$(package sox)/bin/sox"
[ -x "$bundle/bin/semu" ] || { echo "FAIL: no bundle" | tee "$out/result"; exit 1; }
echo "bundle $bundle" | tee "$out/bundle"
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe LIBGL_DRIVERS_PATH="$mesa/lib/dri" __GLX_VENDOR_LIBRARY_NAME=mesa
export __EGL_VENDOR_LIBRARY_DIRS="$mesa/share/glvnd/egl_vendor.d" LD_LIBRARY_PATH="$mesa/lib" VK_DRIVER_FILES="$(ls "$mesa"/share/vulkan/icd.d/lvp_icd*.json | head -1)"
export QT_QPA_PLATFORM=xcb XDG_RUNTIME_DIR=/run/user/0
unset WAYLAND_DISPLAY PULSE_SERVER SDL_AUDIODRIVER SDL_AUDIO_DRIVER ALSA_CONFIG_PATH
mkdir -p "$XDG_RUNTIME_DIR/pulse" && chmod 700 "$XDG_RUNTIME_DIR"
"$pulse/bin/pulseaudio" -n --daemonize=no --exit-idle-time=-1 --use-pid-file=no --disallow-exit \
  -L "module-null-sink sink_name=semu_null" -L "module-native-protocol-unix auth-anonymous=1 socket=$XDG_RUNTIME_DIR/pulse/native" >"$out/pulse.log" 2>&1 &
sleep 3
"$pulse/bin/pactl" info > "$out/pulse-info.txt" 2>&1 || true
display=:94
"$xvfb/bin/Xvfb" "$display" -screen 0 1280x800x24 >"$out/xvfb.log" 2>&1 & xvfb_pid=$!
sleep 2
DISPLAY="$display" "$openbox/bin/openbox" --sm-disable >"$out/openbox.log" 2>&1 &
sleep 2
streams() {  # the sink inputs (application, media, sink, sample spec, corked, volume) and the null sink's peak over one second
  local seen peak
  seen="$("$pulse/bin/pactl" list sink-inputs 2>/dev/null | grep -E 'application.name = |media.name = |Sink: |Sample Specification: |Corked: |Volume: front' | tr -s ' \t' ' ' | paste -sd ' ' -)"
  [ -n "$seen" ] || return 0
  peak="$(timeout 1 "$pulse/bin/parec" -d semu_null.monitor --raw --format=s16le --rate=48000 --channels=2 2>/dev/null \
    | "$sox" -t raw -r 48000 -e signed -b 16 -c 2 - -n stat 2>&1 | grep 'Maximum amplitude' | tr -s ' ' | cut -d' ' -f3)"
  printf '%s peak=%s\n' "$seen" "${peak:-none}"
}
watch() {  # label: polls until WAIT; records the first poll that saw a stream
  local label="$1" elapsed=0 first=""
  : > "$out/$label.streams"
  while [ "$elapsed" -lt "${WAIT:-180}" ]; do
    sleep "${POLL:-5}"; elapsed=$((elapsed + ${POLL:-5}))
    seen="$(streams)"
    printf '%s\t%s\n' "$elapsed" "$seen" >> "$out/$label.streams"
    [ -n "$seen" ] && [ -z "$first" ] && first="$elapsed"
  done
  DISPLAY="$display" "$xwd/bin/xwd" -root -silent | "$magick" xwd:- "$out/$label.png" || true
  echo "${first:-none}"
}
number=0
while IFS="$(printf '\t')" read -r emulator system rom control; do
  number=$((number + 1))
  label="$number-$emulator-$system"
  root="$(mktemp -d)"
  mkdir -p "$root/home" "$root/bios"
  settings="{\"paths\":{\"roms\":\"/roms\",\"state_root\":\"$root/state\",\"content_root\":\"$root/content\",\"emulation_root\":\"/emulation\",\"bios\":\"$root/bios\"},\"visual\":{\"display\":{\"width\":1280,\"height\":800}}}"
  HOME="$root/home" "$bundle/bin/semu" launch "$emulator" --system "$system" --rom "$rom" --settings-json "$settings" --semu-home "$root/home/semu" --print-plan > "$out/$label.plan.json" 2>&1 || true
  HOME="$root/home" DISPLAY="$display" "$bundle/bin/semu" launch "$emulator" --system "$system" --rom "$rom" \
    --settings-json "$settings" --semu-home "$root/home/semu" > "$out/$label.log" 2>&1 &
  launcher=$!
  first="$(watch "$label")"
  kill -TERM "$launcher" 2>/dev/null || true
  wait "$launcher" 2>/dev/null || true
  state="$root/state/$emulator"
  {
    echo "emulator=$emulator system=$system rom=$rom first_stream_after_s=$first"
    echo "loudest_peak=$(grep -o 'peak=[0-9.]*' "$out/$label.streams" | cut -d= -f2 | sort -g | tail -1) (full scale 1.0; 0 is silence)"
    echo "streams_seen:"; grep -v "$(printf '\t')\$" "$out/$label.streams" | head -3 | while IFS= read -r line; do printf '  %s\n' "$line"; done
    echo "cemu_audio_block:"; grep -A 12 '<Audio>' "$state/config/Cemu/settings.xml" 2>/dev/null | while IFS= read -r line; do printf '  %s\n' "$line"; done
    echo "emulator_log:"; cat "$state"/data/Cemu/log.txt "$out/$label.log" 2>/dev/null | grep -aiE 'audio|cubeb|can.t initialize' | sort -u | head -20 | while IFS= read -r line; do printf '  %s\n' "$line"; done
  } > "$out/$label.result"
  if [ "$control" = control ] && [ -f "$state/config/Cemu/settings.xml" ]; then  # same argv and environment, no Audio block
    grep -v -e '<Audio>' -e '</Audio>' -e '<api>3</api>' -e '<delay>' -e 'Channels>' -e 'Volume>' -e 'Device>' "$state/config/Cemu/settings.xml" > "$root/settings-without-audio.xml"
    cp "$root/settings-without-audio.xml" "$state/config/Cemu/settings.xml"
    mapfile -t environment < <("$jq" -r '.environment[]' "$out/$label.plan.json")
    mapfile -t argv < <("$jq" -r '.argv[]' "$out/$label.plan.json")
    HOME="$root/home" DISPLAY="$display" env "${environment[@]}" "${argv[@]}" > "$out/$label-control.log" 2>&1 &
    direct=$!
    control_first="$(watch "$label-control")"
    kill -TERM "$direct" 2>/dev/null || true
    wait "$direct" 2>/dev/null || true
    {
      echo "control (settings.xml without <Audio>, same argv and environment): first_stream_after_s=$control_first"
      echo "control_log:"; cat "$state"/data/Cemu/log.txt 2>/dev/null | grep -aiE 'audio|cubeb|Title|can.t initialize' | sort -u | head -12 | while IFS= read -r line; do printf '  %s\n' "$line"; done
    } >> "$out/$label.result"
  fi
  cat "$out/$label.result"
done < "$out/cases"
kill "$xvfb_pid" 2>/dev/null || true
cat "$out"/*.result > "$out/result"
echo "audio: done; read $out/result (a stream must appear, and stay missing in a control) and judge the captures"
