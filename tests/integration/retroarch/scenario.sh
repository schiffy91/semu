#!/usr/bin/env bash
set -euo pipefail

runtime="${1:?runtime path required}"
results="${2:?results path required}"
here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
repository="$(CDPATH= cd -- "$here/../../.." && pwd -P)"
export PATH="$runtime/bin:$PATH"
export HOME="$results/home"
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_CACHE_HOME="$HOME/.cache"
export DISPLAY=:99
export GALLIUM_DRIVER=llvmpipe
export LIBGL_ALWAYS_SOFTWARE=1
export LIBGL_DRIVERS_PATH="$runtime/lib/dri"
export LD_LIBRARY_PATH="$runtime/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export __GLX_VENDOR_LIBRARY_NAME=mesa
export SEMU_RENDER_STATE_DIR="$results"
export SEMU_SYNTHETIC_CORE_LOG="$results/synthetic-core.log"
export SEMU_RENDER_CAPTURE_FRAME=30
export SEMU_RENDER_SYSTEM_ID=nds
export SEMU_RENDER_EMULATOR_ID=retroarch
export SEMU_RENDER_SHADERS=0
export SEMU_RENDER_BEZELS=0
export SEMU_RENDER_INTEGER_SCALE=0
export SEMU_RENDER_SURFACE_COUNT=2
export SEMU_RENDER_SURFACE_LAYOUT=vertical
export SEMU_RENDER_SURFACE_0_ID=top
export SEMU_RENDER_SURFACE_0_NATIVE=256x192
export SEMU_RENDER_SURFACE_1_ID=bottom
export SEMU_RENDER_SURFACE_1_NATIVE=256x192
export SEMU_RENDER_SURFACE_NATIVE_WIDTH_POLICY=equal
export SEMU_RENDER_SURFACE_GEOMETRY_POLICY=uniform_integer_scale
export SEMU_RENDER_TOUCH_SURFACE_ID=bottom
export SEMU_RENDER_TOUCH_SURFACE_INDEX=1

mkdir -p "$HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$results/saves" "$results/states" "$results/screenshots"
: > "$results/scenario.log"

display_pid=
retroarch_pid=
cleanup() {
  if [[ -n "${retroarch_pid:-}" ]] && kill -0 "$retroarch_pid" 2>/dev/null; then
    kill "$retroarch_pid" 2>/dev/null || true
    for _ in $(seq 1 30); do
      kill -0 "$retroarch_pid" 2>/dev/null || break
      sleep 0.1
    done
    kill -KILL "$retroarch_pid" 2>/dev/null || true
    wait "$retroarch_pid" 2>/dev/null || true
  fi
  if [[ -n "${display_pid:-}" ]] && kill -0 "$display_pid" 2>/dev/null; then
    kill "$display_pid" 2>/dev/null || true
    wait "$display_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT

wait_for() {
  local description="$1"
  shift
  for _ in $(seq 1 200); do
    if "$@"; then
      return 0
    fi
    sleep 0.1
  done
  printf 'FAIL: timed out waiting for %s\n' "$description" >&2
  return 1
}

display_ready() {
  xdpyinfo -display "$DISPLAY" >/dev/null 2>&1
}

cat > "$results/xorg.conf" <<EOF
Section "ServerFlags"
  Option "AutoAddDevices" "false"
  Option "AllowMouseOpenFail" "true"
EndSection
Section "Files"
  ModulePath "$runtime/lib/xorg/modules"
EndSection
Section "Device"
  Identifier "SemuDummy"
  Driver "dummy"
  VideoRam 256000
EndSection
Section "InputDevice"
  Identifier "SemuPointer"
  Driver "void"
  Option "CorePointer" "true"
EndSection
Section "Monitor"
  Identifier "SemuMonitor"
  HorizSync 5.0-1000.0
  VertRefresh 5.0-200.0
  Modeline "1280x800" 83.50 1280 1352 1480 1680 800 803 809 831
EndSection
Section "Screen"
  Identifier "SemuScreen"
  Device "SemuDummy"
  Monitor "SemuMonitor"
  DefaultDepth 24
  SubSection "Display"
    Depth 24
    Modes "1280x800"
  EndSubSection
EndSection
EOF
Xorg "$DISPLAY" -config "$results/xorg.conf" -logfile "$results/Xorg.99.log" +iglx -noreset -nolisten tcp > "$results/xorg.log" 2>&1 &
display_pid=$!
wait_for 'headless Xorg display' display_ready

capture_count_at_least() {
  local expected="$1"
  [[ "$(capture_count)" -ge "$expected" ]]
}

capture_count() {
  local evidence="$results/semu-render-evidence.log"
  if [[ ! -f "$evidence" ]]; then
    printf '0'
    return
  fi
  grep -c 'capture_result=written' "$evidence" || true
}

context_count_at_least() {
  local expected="$1"
  local evidence="$results/semu-render-evidence.log"
  [[ -f "$evidence" ]] || return 1
  local count
  count="$(sed -n 's/.*context_generation=\([0-9][0-9]*\).*/\1/p' \
    "$evidence" | sort -u | wc -l)"
  [[ "$count" -ge "$expected" ]]
}

raw_pointer_pressed() {
  [[ -f "$SEMU_SYNTHETIC_CORE_LOG" ]] &&
    grep -Eq ' raw=[^,]+,[^,]+,1([[:space:]]|$)' "$SEMU_SYNTHETIC_CORE_LOG"
}

mapped_pointer_pressed() {
  [[ -f "$SEMU_SYNTHETIC_CORE_LOG" ]] &&
    grep -Eq ' p0=[^,]+,[^,]+,1([[:space:]]|$)' "$SEMU_SYNTHETIC_CORE_LOG"
}

retroarch_window() {
  xdotool search --onlyvisible --class retroarch 2>/dev/null | tail -1
}

retroarch_window_ready() {
  [[ -n "$(retroarch_window)" ]]
}

retroarch_stopped() {
  local state
  [[ ! -r "/proc/$retroarch_pid/stat" ]] && return 0
  state="$(awk '{ print $3 }' "/proc/$retroarch_pid/stat" 2>/dev/null || true)"
  [[ -z "$state" || "$state" == Z ]]
}

menu_state() {
  local reply
  reply="$(printf 'GET_CONFIG_PARAM menu_active\n' | nc -u -w 1 127.0.0.1 55355 2>/dev/null || true)"
  case "$reply" in
    *'GET_CONFIG_PARAM menu_active true'*) printf 'true' ;;
    *'GET_CONFIG_PARAM menu_active false'*) printf 'false' ;;
    *) printf 'unknown' ;;
  esac
}

menu_is() {
  [[ "$(menu_state)" == "$1" ]]
}

renderer_name="$(glxinfo -B | sed -n 's/^[[:space:]]*OpenGL renderer string: //p' | head -1)"
case "$renderer_name" in
  *llvmpipe*) ;;
  *)
    printf 'FAIL: expected llvmpipe, got %s\n' "$renderer_name" >&2
    exit 1
    ;;
esac

retroarch_binary="$(readlink -f "$runtime/bin/retroarch-semu")"
retroarch_store="/nix/store/$(basename "$(dirname "$(dirname "$retroarch_binary")")")"
{
  printf 'renderer=llvmpipe\n'
  printf 'renderer_string=%s\n' "$renderer_name"
  printf 'retroarch_store=%s\n' "$retroarch_store"
  printf 'retroarch_sha256=%s\n' "$(sha256sum "$retroarch_binary" | cut -d' ' -f1)"
  ldd "$retroarch_binary" | grep 'libsemurenderer.so'
  find "$retroarch_store/share/semu" -name '*build-contract.json' -type f -print -exec cat {} \;
  xinput list
} > "$results/package.log"

cat > "$results/retroarch.cfg" <<EOF
config_save_on_exit = "false"
audio_driver = "null"
audio_sync = "false"
input_driver = "x"
input_joypad_driver = "null"
input_menu_toggle = "f1"
input_toggle_fullscreen = "f"
input_exit_emulator = "escape"
menu_driver = "rgui"
menu_pause_libretro = "false"
network_cmd_enable = "true"
network_cmd_port = "55355"
video_driver = "glcore"
video_context_driver = "x"
video_fullscreen = "true"
video_windowed_fullscreen = "false"
video_threaded = "false"
video_vsync = "false"
video_smooth = "false"
video_shader_enable = "false"
video_scale_integer = "false"
input_overlay_enable = "false"
savestate_directory = "$results/states"
savefile_directory = "$results/saves"
screenshot_directory = "$results/screenshots"
EOF
grep -Fxq 'video_threaded = "false"' "$results/retroarch.cfg"
printf 'video_threaded=false\n' >> "$results/scenario.log"
printf 'synthetic\n' > "$results/content.semu"

"$runtime/bin/retroarch-semu" --verbose --config "$results/retroarch.cfg" -L "$runtime/lib/libretro/semu_synthetic_libretro.so" "$results/content.semu" > "$results/retroarch.log" 2>&1 &
retroarch_pid=$!

wait_for 'RetroArch window' retroarch_window_ready
wait_for 'initial renderer capture' capture_count_at_least 1
cp "$results/semu-render-final.ppm" "$results/frame-before-menu.ppm"

window="$(retroarch_window)"
xdotool mousemove_relative -- 500 300
sleep 0.2
xdotool mousemove_relative -- 0 200
sleep 0.2
xdotool mousedown 1
wait_for 'raw X pointer press' raw_pointer_pressed
wait_for 'Semu-mapped lower-surface press' mapped_pointer_pressed
xdotool mousemove_relative -- 40 40
sleep 0.3
xdotool mouseup 1
sleep 0.3
xdotool mousemove 100 100
xdotool mousedown 1
sleep 0.3
xdotool mouseup 1

wait_for 'network command server' menu_is false
before_reinit_capture="$(capture_count)"
printf 'FULLSCREEN_TOGGLE\n' | nc -u -w 1 127.0.0.1 55355 >/dev/null 2>&1 || true
wait_for 'second GL context generation' context_count_at_least 2
printf 'FULLSCREEN_TOGGLE\n' | nc -u -w 1 127.0.0.1 55355 >/dev/null 2>&1 || true
wait_for 'third GL context generation' context_count_at_least 3
"$runtime/bin/semu-retroarch-control" "$results" 1 screenshot
wait_for 'post-reinit renderer capture' capture_count_at_least \
  "$((before_reinit_capture + 1))"
cp "$results/semu-render-final.ppm" "$results/frame-after-reinit.ppm"

printf 'menu_before=false\n' >> "$results/scenario.log"
printf 'MENU_TOGGLE\n' | nc -u -w 1 127.0.0.1 55355 >/dev/null 2>&1 || true
wait_for 'native menu open' menu_is true
printf 'menu_open=true\n' >> "$results/scenario.log"
window="$(retroarch_window)"
xwd -silent -id "$window" | magick xwd:- -depth 8 ppm:"$results/native-menu.ppm"
before_menu_capture="$(capture_count)"
"$runtime/bin/semu-retroarch-control" "$results" 2 screenshot
if wait_for 'native-menu renderer capture' capture_count_at_least \
    "$((before_menu_capture + 1))"; then
  cp "$results/semu-render-final.ppm" "$results/frame-under-menu.ppm"
  printf 'menu_renderer_capture=true\n' >> "$results/scenario.log"
else
  printf 'menu_renderer_capture=false\n' >> "$results/scenario.log"
fi
printf 'MENU_TOGGLE\n' | nc -u -w 1 127.0.0.1 55355 >/dev/null 2>&1 || true
if wait_for 'native menu close' menu_is false; then
  printf 'menu_closed=false\n' >> "$results/scenario.log"
else
  printf 'menu_closed=unknown\n' >> "$results/scenario.log"
fi

before_menu_capture="$(capture_count)"
"$runtime/bin/semu-retroarch-control" "$results" 3 screenshot
if wait_for 'post-menu renderer capture' capture_count_at_least \
    "$((before_menu_capture + 1))"; then
  cp "$results/semu-render-final.ppm" "$results/frame-after-menu.ppm"
  printf 'menu_resume=true\n' >> "$results/scenario.log"
else
  printf 'menu_resume=false\n' >> "$results/scenario.log"
fi

kill -INT "$retroarch_pid"
wait_for 'RetroArch process exit' retroarch_stopped
set +e
wait "$retroarch_pid"
retroarch_status=$?
set -e
retroarch_pid=
glx_shutdown_clean=true
if grep -Fq 'BadAccess' "$results/retroarch.log"; then
  glx_shutdown_clean=false
  printf 'FAIL: GLX BadAccess occurred during context shutdown\n' >&2
fi
printf 'glx_shutdown_clean=%s\n' "$glx_shutdown_clean" \
  >> "$results/scenario.log"

set +e
"$runtime/bin/semu-retroarch-verify" "$results"
verify_status=$?
set -e
glass_status=1
if [[ "$retroarch_status" -eq 0 && "$glx_shutdown_clean" == true \
    && "$verify_status" -eq 0 ]]; then
  set +e
  "$here/glass_scenario.sh" "$runtime" "$results/glass" "$repository"
  glass_status=$?
  set -e
fi
if [[ "$retroarch_status" -ne 0 ]]; then
  printf 'FAIL: RetroArch exited with status %s after GL context recreation\n' \
    "$retroarch_status" >&2
fi
[[ "$retroarch_status" -eq 0 && "$glx_shutdown_clean" == true \
  && "$verify_status" -eq 0 && "$glass_status" -eq 0 ]]
