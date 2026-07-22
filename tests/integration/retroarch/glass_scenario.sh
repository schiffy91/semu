#!/usr/bin/env bash
set -euo pipefail

runtime="${1:?runtime path required}"
results="${2:?results path required}"
repository="${3:?repository path required}"
mkdir -p "$results"

glass="$("$runtime/bin/semu-retroarch-glass-fixture" \
  "$repository" "$results")"
[[ -n "$glass" && -f "$glass" && -f "$results/declaration.json" \
  && -f "$results/test-bezel.png" ]]

export SEMU_RENDER_EMULATOR_ID=retroarch
export SEMU_RENDER_SYSTEM_ID=gb
export SEMU_RENDER_CAPTURE_FRAME=30
export SEMU_RENDER_SHADERS=0
export SEMU_RENDER_BEZELS=1
export SEMU_RENDER_INTEGER_SCALE=0
export SEMU_RENDER_REFLECT=1
export SEMU_RENDER_ART="$results/test-bezel.png"
export SEMU_RENDER_SURFACE_COUNT=2
export SEMU_RENDER_SURFACE_LAYOUT=vertical
export SEMU_RENDER_SURFACE_0_ID=top
export SEMU_RENDER_SURFACE_0_NATIVE=256x192
export SEMU_RENDER_SURFACE_1_ID=bottom
export SEMU_RENDER_SURFACE_1_NATIVE=256x192
export SEMU_RENDER_SURFACE_NATIVE_WIDTH_POLICY=equal
export SEMU_RENDER_SURFACE_GEOMETRY_POLICY=uniform_integer_scale
export SEMU_RENDER_SCREEN_PRI=0.25,0.54,0.50,0.36
export SEMU_RENDER_SCREEN_SEC=0.25,0.10,0.50,0.36
export SEMU_SYNTHETIC_STATIC=1

retroarch_pid=
cleanup() {
  if [[ -n "${retroarch_pid:-}" ]] \
      && kill -0 "$retroarch_pid" 2>/dev/null; then
    kill -KILL "$retroarch_pid" 2>/dev/null || true
    wait "$retroarch_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT

wait_for() {
  local description="$1"
  shift
  for _ in $(seq 1 200); do
    if "$@"; then return 0; fi
    sleep 0.1
  done
  printf 'FAIL: timed out waiting for %s\n' "$description" >&2
  return 1
}

state=
capture_count() {
  local evidence="$state/semu-render-evidence.log"
  [[ -f "$evidence" ]] || { printf '0'; return; }
  grep -c 'capture_result=written' "$evidence" || true
}

capture_count_at_least() { [[ "$(capture_count)" -ge "$1" ]]; }

retroarch_window_ready() {
  xdotool search --onlyvisible --class retroarch >/dev/null 2>&1
}

menu_is() {
  local reply
  reply="$(printf 'GET_CONFIG_PARAM menu_active\n' \
    | nc -u -w 1 127.0.0.1 55356 2>/dev/null || true)"
  [[ "$reply" == *"GET_CONFIG_PARAM menu_active $1"* ]]
}

retroarch_stopped() {
  local process="$1"
  local process_state
  [[ ! -r "/proc/$process/stat" ]] && return 0
  process_state="$(awk '{ print $3 }' "/proc/$process/stat" \
    2>/dev/null || true)"
  [[ -z "$process_state" || "$process_state" == Z ]]
}

run_case() {
  local name="$1"
  local glass_enabled="$2"
  state="$results/$name"
  export SEMU_RENDER_STATE_DIR="$state"
  export SEMU_SYNTHETIC_CORE_LOG="$state/synthetic-core.log"
  if [[ "$glass_enabled" == true ]]; then
    export SEMU_RENDER_GLASS="$glass"
  else
    unset SEMU_RENDER_GLASS
  fi

  "$runtime/bin/retroarch-semu" --verbose \
    --config "$state/retroarch.cfg" \
    -L "$runtime/lib/libretro/semu_synthetic_libretro.so" \
    "$state/content.semu" > "$state/retroarch.log" 2>&1 &
  retroarch_pid=$!
  wait_for "$name RetroArch window" retroarch_window_ready
  wait_for "$name game capture" capture_count_at_least 1
  cp "$state/semu-render-final.ppm" "$state/game.ppm"
  wait_for "$name command server" menu_is false
  printf 'MENU_TOGGLE\n' | nc -u -w 1 127.0.0.1 55356 \
    >/dev/null 2>&1 || true
  wait_for "$name native menu" menu_is true
  local before_menu_capture
  before_menu_capture="$(capture_count)"
  "$runtime/bin/semu-retroarch-control" "$state" 1 screenshot
  wait_for "$name menu capture" capture_count_at_least \
    "$((before_menu_capture + 1))"
  cp "$state/semu-render-final.ppm" "$state/menu.ppm"
  kill -INT "$retroarch_pid"
  wait_for "$name RetroArch exit" retroarch_stopped "$retroarch_pid"
  set +e
  wait "$retroarch_pid"
  local status=$?
  set -e
  retroarch_pid=
  [[ "$status" -eq 0 ]]
}

run_case control false
run_case treatment true
"$runtime/bin/semu-retroarch-verify" glass "$results"
