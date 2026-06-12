#!/usr/bin/env bash
set -u

APP="${SEMU_APPIMAGE:-/home/deck/Applications/Semu/Semu-x86_64.AppImage}"
PROJECT="${SEMU_PROJECT:-/home/deck/semu}"
ROMS="${SEMU_ROMS:-/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs}"
RESULT="${SEMU_RESULT:-/home/deck/.cache/semu-codex-src/result}"
OUT="${SEMU_TEST_OUT:-/home/deck/.cache/semu-codex-emulator-loop}"
SEND="${SEMU_UINPUT_SEND:-$OUT/uinput-send}"
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
FAILURES=0
BASELINE="$OUT/baseline.png"
CASE_FILTER="${SEMU_CASES:-}"

mkdir -p "$OUT"
: > "$OUT/summary.tsv"

discover_graphical_env() {
  local kwin_pid kwin_xauth
  kwin_pid="$(pgrep -f '/usr/bin/kwin_wayland( |$)' | head -1 || true)"
  if [ -n "$kwin_pid" ] && [ -r "/proc/$kwin_pid/environ" ]; then
    while IFS= read -r line; do
      case "$line" in
        DISPLAY=*|XAUTHORITY=*|WAYLAND_DISPLAY=*|XDG_RUNTIME_DIR=*|DBUS_SESSION_BUS_ADDRESS=*) export "$line" ;;
      esac
    done < <(tr '\0' '\n' < "/proc/$kwin_pid/environ")
  fi
  if [ -z "${XAUTHORITY:-}" ]; then
    kwin_xauth="$(pgrep -a kwin_wayland | awk '{ for (i=1; i<NF; i++) if ($i == "--xwayland-xauthority") { print $(i+1); exit } }' || true)"
    [ -n "$kwin_xauth" ] && export XAUTHORITY="$kwin_xauth"
  fi
  if [ -z "${XAUTHORITY:-}" ]; then
    for candidate in /run/user/1000/xauth_*; do
      [ -r "$candidate" ] && export XAUTHORITY="$candidate" && break
    done
  fi
  export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/1000}"
  export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/1000/bus}"
  export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
  export DISPLAY="${DISPLAY:-:0}"
}

discover_graphical_env

cleanup_emulators() {
  local name
  for name in \
    retroarch dolphin-emu ppsspp flycast melonDS pcsx2-qt Cemu azahar Ryujinx es-de; do
    pkill -TERM -x "$name" 2>/dev/null || true
  done
  sleep 1
  for name in \
    retroarch dolphin-emu ppsspp flycast melonDS pcsx2-qt Cemu azahar Ryujinx es-de; do
    pkill -KILL -x "$name" 2>/dev/null || true
  done
}

cleanup_emulators
ps -eo pid,args | awk '/\/usr\/bin\/es-de --no-splash/ {print $1}' | xargs -r kill 2>/dev/null || true
sleep 2

build_uinput_sender() {
  if [ -x "$SEND" ]; then
    return 0
  fi
  if [ -f "$SCRIPT_DIR/uinput-send.c" ] && command -v cc >/dev/null 2>&1; then
    cc "$SCRIPT_DIR/uinput-send.c" -O2 -Wall -Wextra -o "$SEND" && return 0
  fi
  local nix_cmd=""
  if command -v nix >/dev/null 2>&1; then
    nix_cmd="$(command -v nix)"
  elif [ -x /nix/var/nix/profiles/default/bin/nix ]; then
    nix_cmd=/nix/var/nix/profiles/default/bin/nix
  fi
  if [ -n "$nix_cmd" ] && [ -f "$SCRIPT_DIR/uinput-send.c" ]; then
    (
      cd "$PROJECT"
      "$nix_cmd" --extra-experimental-features "nix-command flakes" develop -c \
        cc "$SCRIPT_DIR/uinput-send.c" -O2 -Wall -Wextra -o "$SEND"
    ) && return 0
  fi
  if [ -x /home/deck/.cache/semu-uinput-send ]; then
    SEND=/home/deck/.cache/semu-uinput-send
    return 0
  fi
  return 1
}

capture_screen() {
  local path="$1"
  spectacle -b -n -o "$path" >/tmp/semu-loop-spectacle.log 2>&1 || cat /tmp/semu-loop-spectacle.log >&2
}

file_sha() {
  local path="$1"
  [ -f "$path" ] || return 1
  sha256sum "$path" | awk '{print $1}'
}

window_snapshot() {
  local path="$1"
  {
    echo "DISPLAY=${DISPLAY:-}"
    echo "XAUTHORITY=${XAUTHORITY:-}"
    if command -v xdotool >/dev/null 2>&1; then
      xdotool getactivewindow getwindowname 2>/dev/null || true
      xdotool search --onlyvisible --name '.*' getwindowname %@ 2>/dev/null || true
    fi
  } > "$path"
}

focus_emulator_window() {
  local emulator="$1"
  command -v xdotool >/dev/null 2>&1 || return 1
  local patterns=()
  case "$emulator" in
    retroarch) patterns=("RetroArch") ;;
    dolphin) patterns=("Dolphin") ;;
    ppsspp) patterns=("PPSSPP") ;;
    flycast) patterns=("Flycast") ;;
    melonds) patterns=("melonDS") ;;
    pcsx2) patterns=("PCSX2") ;;
    cemu) patterns=("Cemu" "CEMU") ;;
    azahar) patterns=("Azahar") ;;
    ryujinx) patterns=("Ryujinx") ;;
    *) patterns=("$emulator") ;;
  esac
  local pattern win
  for pattern in "${patterns[@]}"; do
    win="$(xdotool search --onlyvisible --name "$pattern" 2>/dev/null | tail -1 || true)"
    if [ -n "$win" ]; then
      xdotool windowactivate "$win" >/dev/null 2>&1 || xdotool windowraise "$win" >/dev/null 2>&1 || true
      return 0
    fi
  done
  return 1
}

wait_for_emulator_window() {
  local emulator="$1"
  local attempts="${2:-12}"
  local i=0
  while [ "$i" -lt "$attempts" ]; do
    if focus_emulator_window "$emulator"; then
      return 0
    fi
    sleep 1
    i=$((i + 1))
  done
  return 1
}

alive() {
  kill -0 "$1" 2>/dev/null
}

send_input() {
  local action
  for action in "$@"; do
    case "$action" in
      xkey-*)
        command -v xdotool >/dev/null 2>&1 && xdotool key "${action#xkey-}" >/dev/null 2>&1 || true
        ;;
      xclick)
        command -v xdotool >/dev/null 2>&1 && xdotool click 1 >/dev/null 2>&1 || true
        ;;
      *)
        [ -x "$SEND" ] && sudo "$SEND" "$action" >/dev/null 2>&1 || true
        ;;
    esac
    sleep 0.1
  done
}

input_probe_actions() {
  case "$1" in
    n3ds) printf '%s\n' key-a ;;
    ps2) printf '%s\n' enter ;;
    wii) printf '%s\n' steam-a xkey-Return xclick ;;
    *) printf '%s\n' gameplay-probe ;;
  esac
}

case_selected() {
  local id="$1"
  [ -z "$CASE_FILTER" ] && return 0
  case ",$CASE_FILTER," in
    *,"$id",*) return 0 ;;
    *) return 1 ;;
  esac
}

case_failed() {
  local required="$1"
  local id="$2"
  local status="$3"
  local quit="$4"
  local size="$5"
  local visual="$6"
  local input_visual="$7"
  if [ "$status" != "0" ] || [ "$quit" != "ok" ] || [ "$size" -lt 50000 ] || [ "$visual" != "changed" ] || [ "$input_visual" != "changed" ]; then
    printf 'FAIL\t%s\tstatus=%s\tquit=%s\tpng_bytes=%s\tvisual=%s\tinput_visual=%s\trequired=%s\n' "$id" "$status" "$quit" "$size" "$visual" "$input_visual" "$required" | tee -a "$OUT/summary.tsv"
    if [ "$required" = "yes" ]; then
      FAILURES=$((FAILURES + 1))
    fi
  fi
}

run_case() {
  local id="$1"
  local emulator="$2"
  local executable="$3"
  local wait_seconds="$4"
  shift 4

  case_selected "$id" || return 0

  local log="$OUT/$id.log"
  local case_baseline="$OUT/$id.baseline.png"
  local png="$OUT/$id.png"
  local input_png="$OUT/$id.after-input.png"
  local status="unknown"
  local quit="unknown"
  local size="0"
  local case_baseline_sha=""
  local sha=""
  local input_sha=""
  local visual="unknown"
  local input_visual="not-run"
  local input_probe=""
  local quit_debug="${SEMU_QUIT_WATCH_DEBUG:-0}"
  local required="${SEMU_CURRENT_ROUTE_REQUIRED:-yes}"

  rm -f "$log" "$case_baseline" "$png" "$input_png"
  printf 'START\t%s\t%s\n' "$id" "$(date +%H:%M:%S)" | tee -a "$OUT/summary.tsv"
  capture_screen "$case_baseline"
  case_baseline_sha="$(file_sha "$case_baseline" || true)"

  setsid env \
    XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    DBUS_SESSION_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS" \
    WAYLAND_DISPLAY="$WAYLAND_DISPLAY" \
    DISPLAY="$DISPLAY" \
    XAUTHORITY="$XAUTHORITY" \
    SEMU_PROJECT_DIR="$PROJECT" \
    SEMU_ROMS="$ROMS" \
    SEMU_ROMS_DIR="$ROMS" \
    SEMU_QUIT_WATCH_DEBUG="$quit_debug" \
    "$APP" launcher routed "$emulator" "$executable" "$@" >"$log" 2>&1 < /dev/null &

  local pid=$!
  sleep "$wait_seconds"
  wait_for_emulator_window "$emulator" 12 || true
  sleep 1
  window_snapshot "$OUT/$id.windows"
  capture_screen "$png"
  [ -f "$png" ] && size="$(wc -c < "$png" | tr -d ' ')"
  sha="$(file_sha "$png" || true)"
  if ! alive "$pid"; then
    visual="not-running"
  elif [ -n "$sha" ] && [ -n "$case_baseline_sha" ] && [ "$sha" != "$case_baseline_sha" ]; then
    visual="changed"
  elif [ -n "$sha" ] && [ "$sha" = "$case_baseline_sha" ]; then
    visual="baseline"
  fi

  if alive "$pid"; then
    input_probe="$(input_probe_actions "$id")"
    send_input $input_probe
    sleep 2
    capture_screen "$input_png"
    input_sha="$(file_sha "$input_png" || true)"
    if [ -n "$input_sha" ] && [ -n "$case_baseline_sha" ] && [ "$input_sha" = "$case_baseline_sha" ]; then
      input_visual="baseline"
    elif [ -n "$input_sha" ] && [ -n "$sha" ] && [ "$input_sha" != "$sha" ]; then
      input_visual="changed"
    elif [ -n "$input_sha" ]; then
      input_visual="unchanged"
    fi
  fi

  if alive "$pid"; then
    send_input select-start
    sleep 6
  fi
  if alive "$pid"; then
    send_input esc
    sleep 4
  fi
  if alive "$pid"; then
    kill -- "-$pid" 2>/dev/null || kill "$pid" 2>/dev/null || true
    sleep 2
  fi
  if alive "$pid"; then
    kill -9 -- "-$pid" 2>/dev/null || kill -9 "$pid" 2>/dev/null || true
  fi

  wait "$pid" >/dev/null 2>&1
  status="$?"
  cleanup_emulators
  if grep -q 'quit key: select+start\|quit key: escape\|quit requested' "$log" 2>/dev/null; then
    quit="ok"
  else
    quit="missing"
  fi
  printf 'RESULT\t%s\tstatus=%s\tquit=%s\tpng_bytes=%s\tvisual=%s\tinput_visual=%s\trequired=%s\n' "$id" "$status" "$quit" "$size" "$visual" "$input_visual" "$required" | tee -a "$OUT/summary.tsv"
  case_failed "$required" "$id" "$status" "$quit" "$size" "$visual" "$input_visual"
  tail -20 "$log" > "$OUT/$id.tail" 2>/dev/null || true
  sleep 2
}

run_optional_case() {
  if [ "${SEMU_OPTIONAL_ROUTES:-0}" != "1" ]; then
    return 0
  fi
  SEMU_CURRENT_ROUTE_REQUIRED=no run_case "$@"
}

core() {
  readlink -f "$RESULT/lib/retroarch/cores/$1"
}

exe() {
  readlink -f "$RESULT/bin/$1"
}

build_uinput_sender || true
capture_screen "$BASELINE"
window_snapshot "$OUT/baseline.windows"

run_case gb retroarch "$(exe retroarch)" 12 -L "$(core gambatte_libretro.so)" "$ROMS/gb/Tetris (World) (Rev 1).zip"
run_case gbc retroarch "$(exe retroarch)" 12 -L "$(core gambatte_libretro.so)" "$ROMS/gbc/Game & Watch Gallery 3 (USA, Europe) (SGB Enhanced) (GB Compatible).zip"
run_case gba retroarch "$(exe retroarch)" 12 -L "$(core mgba_libretro.so)" "$ROMS/gba/Mega Man Zero 3 (USA).zip"
run_case nes retroarch "$(exe retroarch)" 12 -L "$(core mesen_libretro.so)" "$ROMS/nes/Bionic Commando (USA).zip"
run_case snes retroarch "$(exe retroarch)" 12 -L "$(core snes9x_libretro.so)" "$ROMS/snes/Super Metroid (Japan, USA) (En,Ja).zip"
run_case genesis retroarch "$(exe retroarch)" 12 -L "$(core genesis_plus_gx_libretro.so)" "$ROMS/genesis/Sonic The Hedgehog (USA, Europe).zip"
run_case n64-retroarch retroarch "$(exe retroarch)" 16 -L "$(core mupen64plus_next_libretro.so)" "$ROMS/n64/Super Smash Bros. (USA).zip"
run_case psp ppsspp "$(exe ppsspp)" 18 "$ROMS/psp/LocoRoco (USA) (En,Ja,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi,Zh,Ko,Ru).iso"
run_case dreamcast flycast "$(exe flycast)" 18 "$ROMS/dreamcast/ChuChu Rocket! (USA) (En,Ja,Fr,De,Es).chd"
run_case gc dolphin "$(exe dolphin-emu)" 22 "$ROMS/gc/Super Monkey Ball 2 (USA).rvz"
run_case wii dolphin "$(exe dolphin-emu)" 24 "$ROMS/wii/Kirby's Epic Yarn (USA) (En,Fr,Es).wbfs"
run_case ps2 pcsx2 "$(exe pcsx2-qt)" 28 "$ROMS/ps2/Devil May Cry (USA).iso"
run_case nds-melonds melonds "$(exe melonDS)" 18 "$ROMS/nds/Castlevania - Dawn of Sorrow (USA).zip"
run_case n3ds azahar "$(exe azahar)" 28 "$ROMS/n3ds/Super Mario 3D Land (USA) (En,Fr,Es) (Rev 1).3ds"
run_case wiiu cemu "$(exe Cemu)" 30 "$ROMS/wiiu/New SUPER MARIO BROS. U (US).wua"
run_case switch ryujinx "$(exe Ryujinx)" 35 "$ROMS/switch/Animal Crossing New Horizons [01006F8002326000][US][v0].nsp"
run_optional_case nds-retroarch retroarch "$(exe retroarch)" 16 -L "$(core desmume_libretro.so)" "$ROMS/nds/Castlevania - Dawn of Sorrow (USA).zip"

echo DONE | tee -a "$OUT/summary.tsv"
if [ "$FAILURES" -gt 0 ]; then
  exit 1
fi
