#!/usr/bin/env bash
set -u

APP="${SEMU_APPIMAGE:-/home/deck/Applications/Semu/Semu-x86_64.AppImage}"
PROJECT="${SEMU_PROJECT:-/home/deck/.local/share/semu}"
ROMS="${SEMU_ROMS:-/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs}"
RESULT="${SEMU_RESULT:-/home/deck/semu/result}"
OUT="${SEMU_TEST_OUT:-/home/deck/.cache/semu-codex-emulator-loop}"
SEND="${SEMU_UINPUT_SEND:-$OUT/uinput-send}"
LAUNCH_MODE="${SEMU_LAUNCH_MODE:-stable}"
ACTIVE_LAUNCHER_BIN="${SEMU_ACTIVE_LAUNCHER_BIN:-}"
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd -P)"
FAILURES=0
BASELINE="$OUT/baseline.png"
CASE_FILTER="${SEMU_CASES:-}"
MIN_PNG_BYTES="${SEMU_MIN_PNG_BYTES:-5000}"

mkdir -p "$OUT"
: > "$OUT/summary.tsv"

case_wait() {
  local id="$1"
  local default="$2"
  local env_name
  local value
  env_name="SEMU_$(printf '%s' "$id" | tr '[:lower:]-' '[:upper:]_')_WAIT"
  eval "value=\${$env_name:-}"
  printf '%s\n' "${value:-$default}"
}

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
  if [ -x "$SEND" ] && [ "$SEND" -nt "$SCRIPT_DIR/uinput-send.c" ]; then
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
      cd "$REPO_ROOT"
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

emulator_process_alive() {
  local emulator="$1"
  local patterns=()
  case "$emulator" in
    retroarch) patterns=("retroarch") ;;
    dolphin) patterns=("dolphin-emu") ;;
    ppsspp) patterns=("PPSSPPSDL" "PPSSPPQt" "ppsspp") ;;
    flycast) patterns=("flycast") ;;
    melonds) patterns=("melonDS") ;;
    pcsx2) patterns=("pcsx2-qt") ;;
    cemu) patterns=("Cemu") ;;
    azahar) patterns=("azahar") ;;
    ryujinx) patterns=("Ryujinx") ;;
    *) patterns=("$emulator") ;;
  esac
  local pattern
  for pattern in "${patterns[@]}"; do
    if pgrep -x "$pattern" >/dev/null 2>&1 || pgrep -f "/$pattern( |$)" >/dev/null 2>&1; then
      return 0
    fi
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
      xclick-at-*)
        if command -v xdotool >/dev/null 2>&1; then
          local coords="${action#xclick-at-}"
          local x="${coords%-*}"
          local y="${coords#*-}"
          xdotool mousemove "$x" "$y" click 1 >/dev/null 2>&1 || true
        fi
        ;;
      *)
        [ -x "$SEND" ] && sudo "$SEND" "$action" >/dev/null 2>&1 || true
        ;;
    esac
    sleep 0.1
  done
}

fifo_send() {
  local fifo="$1"
  local action="$2"
  timeout 2 sh -c 'printf "%s\n" "$1" > "$2"' sh "$action" "$fifo" 2>/dev/null || true
}

allow_synthetic_steam_input_read() {
  local name event
  for name in /sys/class/input/event*/device/name; do
    [ -r "$name" ] || continue
    [ "$(cat "$name")" = "Steam Deck" ] || continue
    event="/dev/input/$(basename "$(dirname "$(dirname "$name")")")"
    [ -e "$event" ] && sudo chmod a+r "$event" 2>/dev/null || true
  done
}

input_probe_actions() {
  case "$1" in
    n3ds) printf '%s\n' key-a ;;
    psp) printf '%s\n' space key-x key-right key-x key-left key-x ;;
    ps2) printf '%s\n' enter ;;
    wii) printf '%s\n' steam-a enter space key-a key-z xkey-Return xkey-a xkey-space xkey-z xclick ;;
    wiiu) printf '%s\n' steam-b steam-a dpad-right dpad-left ;;
    *) printf '%s\n' gameplay-probe ;;
  esac
}

input_probe_wait() {
  case "$1" in
    psp) printf '%s\n' 4 ;;
    *) printf '%s\n' 2 ;;
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
  local render="$8"
  local content="$9"
  if [ "$status" != "0" ] || [ "$quit" != "ok" ] || [ "$size" -lt "$MIN_PNG_BYTES" ] || [ "$visual" != "changed" ] || [ "$input_visual" != "changed" ] || { [ "$render" != "ok" ] && [ "$render" != "not-required" ]; } || [ "$content" != "ok" ]; then
    printf 'FAIL\t%s\tstatus=%s\tquit=%s\tpng_bytes=%s\tvisual=%s\tinput_visual=%s\trender=%s\tcontent=%s\trequired=%s\n' "$id" "$status" "$quit" "$size" "$visual" "$input_visual" "$render" "$content" "$required" | tee -a "$OUT/summary.tsv"
    if [ "$required" = "yes" ]; then
      FAILURES=$((FAILURES + 1))
    fi
  fi
}

render_expected() {
  case "$1" in
    ""|es-de|settings|semu-settings) return 1 ;;
    *) return 0 ;;
  esac
}

render_hook_config() {
  local emulator="$1"
  printf '%s\n' "$PROJECT/.semu/generated/assets/rendering/hooks/$emulator.json"
}

render_implementation_status() {
  local emulator="$1"
  local config
  config="$(render_hook_config "$emulator")"
  if command -v jq >/dev/null 2>&1 && [ -f "$config" ]; then
    jq -r '.implementation_status // "unknown"' "$config" 2>/dev/null || printf '%s\n' unknown
    return 0
  fi
  printf '%s\n' unknown
}

render_requires_deck_proof() {
  local emulator="$1"
  local config
  config="$(render_hook_config "$emulator")"
  if command -v jq >/dev/null 2>&1 && [ -f "$config" ]; then
    jq -e '.requires_deck_proof == true' "$config" >/dev/null 2>&1
    return $?
  fi
  return 1
}

render_effects_required() {
  local system_id="$1"
  local emulator="$2"
  local config
  config="$(render_hook_config "$emulator")"
  if ! command -v jq >/dev/null 2>&1 || [ ! -f "$config" ]; then
    return 0
  fi
  jq -e --arg system "$system_id" '
    .systems[]
    | select(.id == $system)
    | (((.shader_enabled_by_default == true) and ((.shader_effect_file // "") | length > 0))
       or ((.bezel_enabled_by_default == true) and ((.bezel_effect_file // "") | length > 0)))
  ' "$config" >/dev/null 2>&1
}

render_status() {
  local system_id="$1"
  local emulator="$2"
  local log="$3"
  local visual="${4:-unknown}"
  local proof="${5:-}"
  local implementation
  local requires_proof
  if [ "${SEMU_RENDER_REQUIRED:-1}" = "0" ]; then
    printf '%s\n' "not-required"
    return 0
  fi
  if ! render_expected "$system_id"; then
    printf '%s\n' "not-required"
    return 0
  fi
  implementation="$(render_implementation_status "$emulator")"
  requires_proof=0
  if [ "$implementation" = "implemented_source_hook" ] || render_requires_deck_proof "$emulator"; then
    requires_proof=1
  fi
  if ! render_effects_required "$system_id" "$emulator" && [ "$requires_proof" != "1" ]; then
    printf '%s\n' "not-required"
    return 0
  fi
  if grep -q 'host render unavailable\|render wrapper unavailable\|render wrapper fallback\|No display available\|Segmentation fault\|gamescope failed status=\|SDL_CreateWindow failed\|All graphics backends failed\|Cannot initialize the graphics API\|Failed to load reshade fx file\|preprocessor error\|could not open included file' "$log" 2>/dev/null; then
    printf '%s\n' "failed"
    return 0
  fi
  if grep -q 'Surface extension not found' "$log" 2>/dev/null; then
    printf '%s\n' "failed"
    return 0
  fi
  if [ "$visual" != "changed" ]; then
    printf '%s\n' "missing"
    return 0
  fi
  if [ -n "$proof" ] && grep -Fq "semu-render-hook:$emulator:game_framebuffer" "$proof" 2>/dev/null; then
    if [ "$implementation" = "implemented_source_hook" ]; then
      printf '%s\n' "ok"
    else
      printf '%s\n' "proof-only"
    fi
    return 0
  fi
  printf '%s\n' "missing"
}

png_content_status() {
  local png="$1"
  [ -f "$png" ] || return 0
  command -v python3 >/dev/null 2>&1 || return 0
  python3 - "$png" <<'PY'
import struct
import sys
import zlib

path = sys.argv[1]
data = open(path, "rb").read()
if not data.startswith(b"\x89PNG\r\n\x1a\n"):
    sys.exit(0)

pos = 8
width = height = color_type = bit_depth = None
idat = []
while pos + 8 <= len(data):
    length = struct.unpack(">I", data[pos:pos + 4])[0]
    kind = data[pos + 4:pos + 8]
    chunk = data[pos + 8:pos + 8 + length]
    pos += 12 + length
    if kind == b"IHDR":
        width, height, bit_depth, color_type = struct.unpack(">IIBB", chunk[:10])[:4]
    elif kind == b"IDAT":
        idat.append(chunk)
    elif kind == b"IEND":
        break

if bit_depth != 8 or color_type not in (0, 2, 6) or not width or not height:
    sys.exit(0)

channels = {0: 1, 2: 3, 6: 4}[color_type]
stride = width * channels
raw = zlib.decompress(b"".join(idat))
rows = []
offset = 0

def paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c

for _ in range(height):
    filt = raw[offset]
    offset += 1
    row = bytearray(raw[offset:offset + stride])
    offset += stride
    prev = rows[-1] if rows else bytearray(stride)
    for i in range(stride):
        left = row[i - channels] if i >= channels else 0
        up = prev[i]
        up_left = prev[i - channels] if i >= channels else 0
        if filt == 1:
            row[i] = (row[i] + left) & 0xff
        elif filt == 2:
            row[i] = (row[i] + up) & 0xff
        elif filt == 3:
            row[i] = (row[i] + ((left + up) // 2)) & 0xff
        elif filt == 4:
            row[i] = (row[i] + paeth(left, up, up_left)) & 0xff
    rows.append(row)

nonblack = 0
colorful = 0
total = width * height
for row in rows:
    for x in range(0, stride, channels):
        if channels == 1:
            r = g = b = row[x]
        else:
            r, g, b = row[x], row[x + 1], row[x + 2]
        if max(r, g, b) > 24:
            nonblack += 1
        if max(r, g, b) - min(r, g, b) > 12:
            colorful += 1

if nonblack / total < 0.02 and colorful / total < 0.005:
    print("blank")
PY
}

content_status() {
  local log="$1"
  local png="$2"
  if grep -Eiq 'Firmware is missing|failed to load content|content failed to load|No firmware|Missing .*firmware|Missing .*BIOS|Unable to find .*(BIOS|firmware|keys)|encrypted.*ROM|ROM.*encrypted|No product keys|Invalid NCA|Could not load.*keys' "$log" 2>/dev/null; then
    printf '%s\n' "failed"
    return 0
  fi
  if [ "$(png_content_status "$png" 2>/dev/null || true)" = "blank" ]; then
    printf '%s\n' "blank"
    return 0
  fi
  printf '%s\n' "ok"
}

input_status() {
  local log="$1"
  local visual="$2"
  if grep -Eiq 'Unhandled exception caught|System\.NullReferenceException|Object reference not set|Controller Applet.*invalid|current configuration is invalid' "$log" 2>/dev/null; then
    printf '%s\n' "failed"
    return 0
  fi
  if grep -Eiq 'No matching controllers found|No controller configured|No input device detected|Input device .*not found|Could not find .*controller|Failed to open .*controller|Failed to open .*gamepad' "$log" 2>/dev/null \
    && ! grep -Eiq 'Connected Controller .* to Player1|Configured Controller .* to Player1' "$log" 2>/dev/null; then
    printf '%s\n' "failed"
    return 0
  fi
  printf '%s\n' "$visual"
}

run_case() {
  local id="$1"
  local emulator="$2"
  local executable="$3"
  local wait_seconds="$4"
  shift 4

  case_selected "$id" || return 0

  local log="$OUT/$id.log"
  local quit_log="$OUT/$id.quit-watch.log"
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
  local input_fifo=""
  local input_agent_pid=""
  local quit_debug="${SEMU_QUIT_WATCH_DEBUG:-0}"
  local required="${SEMU_CURRENT_ROUTE_REQUIRED:-yes}"
  local system_id="${id%%-*}"
  local window="missing"
  local render="unknown"
  local content="unknown"
  local render_required="0"
  local render_proof="$PROJECT/.semu/generated/assets/rendering/hooks/$emulator.proof"
  if render_expected "$system_id"; then
    render_required="${SEMU_RENDER_REQUIRED:-1}"
  fi

  rm -f "$log" "$quit_log" "$case_baseline" "$png" "$input_png" "$render_proof"
  printf 'START\t%s\t%s\n' "$id" "$(date +%H:%M:%S)" | tee -a "$OUT/summary.tsv"
  capture_screen "$case_baseline"
  case_baseline_sha="$(file_sha "$case_baseline" || true)"

  if [ "$id" = "wiiu" ] && [ -x "$SEND" ]; then
    input_fifo="$OUT/$id.input.fifo"
    rm -f "$input_fifo"
    mkfifo "$input_fifo"
    sudo "$SEND" --fifo "$input_fifo" > "$OUT/$id.input.log" 2>&1 &
    input_agent_pid="$!"
    sleep 1
    allow_synthetic_steam_input_read
  fi

  setsid env \
    XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    DBUS_SESSION_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS" \
    WAYLAND_DISPLAY="$WAYLAND_DISPLAY" \
    DISPLAY="$DISPLAY" \
    XAUTHORITY="$XAUTHORITY" \
    SEMU_PROJECT_DIR="$PROJECT" \
    SEMU_ROMS="$ROMS" \
    SEMU_ROMS_DIR="$ROMS" \
    SEMU_SYSTEM="$system_id" \
    SEMU_ACTIVE_LAUNCHER_BIN="$(active_launcher_bin)" \
    SEMU_RENDER_REQUIRED="$render_required" \
    SEMU_RENDER_HOOK_PROOF="$render_proof" \
    SEMU_QUIT_WATCH_DEBUG="$quit_debug" \
    SEMU_QUIT_WATCH_LOG="$quit_log" \
    "$(launcher_command "$emulator")" $(launcher_prefix_args "$emulator" "$executable") "$@" >"$log" 2>&1 < /dev/null &

  local pid=$!
  sleep "$wait_seconds"
  if wait_for_emulator_window "$emulator" 12; then
    window="found"
  elif emulator_process_alive "$emulator"; then
    window="process"
  elif alive "$pid"; then
    window="process"
  fi
  sleep 1
  window_snapshot "$OUT/$id.windows"
  capture_screen "$png"
  [ -f "$png" ] && size="$(wc -c < "$png" | tr -d ' ')"
  sha="$(file_sha "$png" || true)"
  if [ "$window" = "missing" ]; then
    visual="window-missing"
  elif ! alive "$pid"; then
    visual="not-running"
  elif [ -n "$sha" ] && [ -n "$case_baseline_sha" ] && [ "$sha" != "$case_baseline_sha" ]; then
    visual="changed"
  elif [ -n "$sha" ] && [ "$sha" = "$case_baseline_sha" ]; then
    visual="baseline"
  fi

  if alive "$pid" && [ "$window" != "missing" ]; then
    if [ -n "$input_agent_pid" ]; then
      input_probe="$(input_probe_actions "$id")"
      fifo_send "$input_fifo" "$input_probe"
      send_input xclick-at-640-580 xkey-Return xkey-z xkey-space
      sleep 3
    else
      input_probe="$(input_probe_actions "$id")"
      send_input $input_probe
      sleep "$(input_probe_wait "$id")"
    fi
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
    if [ -n "$input_agent_pid" ]; then
      fifo_send "$input_fifo" select-start
    else
      send_input select-start
    fi
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
  if [ -n "$input_agent_pid" ]; then
    fifo_send "$input_fifo" quit
    wait "$input_agent_pid" >/dev/null 2>&1 || true
    rm -f "$input_fifo"
  fi
  cleanup_emulators
  if grep -q ' quit .*reason=' "$quit_log" 2>/dev/null; then
    quit="ok"
  elif grep -q 'quit key: select+start\|quit key: escape\|quit requested' "$log" 2>/dev/null; then
    quit="ok"
  else
    quit="missing"
  fi
  render="$(render_status "$system_id" "$emulator" "$log" "$visual" "$render_proof")"
  content="$(content_status "$log" "$png")"
  input_visual="$(input_status "$log" "$input_visual")"
  printf 'RESULT\t%s\tstatus=%s\tquit=%s\tpng_bytes=%s\twindow=%s\tvisual=%s\tinput_visual=%s\trender=%s\tcontent=%s\trequired=%s\n' "$id" "$status" "$quit" "$size" "$window" "$visual" "$input_visual" "$render" "$content" "$required" | tee -a "$OUT/summary.tsv"
  case_failed "$required" "$id" "$status" "$quit" "$size" "$visual" "$input_visual" "$render" "$content"
  tail -20 "$log" > "$OUT/$id.tail" 2>/dev/null || true
  tail -40 "$quit_log" > "$OUT/$id.quit-watch.tail" 2>/dev/null || true
  sleep 2
}

launcher_command() {
  local emulator="$1"
  case "$LAUNCH_MODE" in
    stable)
      local active_bin
      active_bin="$(active_launcher_bin)"
      if [ -n "$active_bin" ] && [ -x "$active_bin/semu-btrc" ]; then
        printf '%s\n' "$active_bin/semu-btrc"
        return 0
      fi
      printf '%s\n' "$PROJECT/.semu/generated/bin/semu-$emulator"
      ;;
    appimage|*)
      printf '%s\n' "$APP"
      ;;
  esac
}

launcher_prefix_args() {
  local emulator="$1"
  local executable="${2:-}"
  case "$LAUNCH_MODE" in
    stable)
      local active_bin
      active_bin="$(active_launcher_bin)"
      if [ -n "$active_bin" ] && [ -x "$active_bin/semu-btrc" ]; then
        printf '%s\n' launcher routed "$emulator" "$executable"
      fi
      ;;
    appimage|*)
      printf '%s\n' "semu-$emulator"
      ;;
  esac
}

active_launcher_bin() {
  if [ -n "$ACTIVE_LAUNCHER_BIN" ]; then
    printf '%s\n' "$ACTIVE_LAUNCHER_BIN"
    return 0
  fi
  case "$LAUNCH_MODE" in
    stable)
      printf '%s\n' "$RESULT/bin"
      ;;
    appimage|*)
      printf '%s\n' ""
      ;;
  esac
}

run_optional_case() {
  if [ "${SEMU_OPTIONAL_ROUTES:-0}" != "1" ]; then
    return 0
  fi
  SEMU_CURRENT_ROUTE_REQUIRED=no run_case "$@"
}

core() {
  if [ "$LAUNCH_MODE" = "appimage" ]; then
    printf '%s\n' "$1"
    return 0
  fi
  readlink -f "$RESULT/lib/retroarch/cores/$1"
}

exe() {
  if [ "$LAUNCH_MODE" = "appimage" ]; then
    printf '%s\n' "$1"
    return 0
  fi
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
run_case psx retroarch "$(exe retroarch)" 18 -L "$(core mednafen_psx_libretro.so)" "$ROMS/psx/R4 - Ridge Racer Type 4 (USA).m3u/R4 - Ridge Racer Type 4 (USA).m3u"
run_case psp ppsspp "$(exe ppsspp)" 18 "$ROMS/psp/LocoRoco (USA) (En,Ja,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi,Zh,Ko,Ru).iso"
run_case dreamcast flycast "$(exe flycast)" 18 "$ROMS/dreamcast/ChuChu Rocket! (USA) (En,Ja,Fr,De,Es).chd"
run_case gc dolphin "$(exe dolphin-emu)" 22 "$ROMS/gc/Super Monkey Ball 2 (USA).rvz"
run_case wii dolphin "$(exe dolphin-emu)" 24 "$ROMS/wii/Kirby's Epic Yarn (USA) (En,Fr,Es).wbfs"
run_case ps2 pcsx2 "$(exe pcsx2-qt)" 28 "$ROMS/ps2/Devil May Cry (USA).iso"
run_case nds-melonds melonds "$(exe melonDS)" 18 "$ROMS/nds/Castlevania - Dawn of Sorrow (USA).zip"
run_case n3ds azahar "$(exe azahar)" 28 "$ROMS/n3ds/Super Mario 3D Land (USA) (En,Fr,Es) (Rev 1).3ds"
run_case wiiu cemu "$(exe Cemu)" "$(case_wait wiiu 90)" "${SEMU_WIIU_ROM:-$ROMS/wiiu/Mario Tennis Ultra Smash (US).wua}"
run_case switch ryujinx "$(exe Ryujinx)" "$(case_wait switch 300)" "$ROMS/switch/Animal Crossing New Horizons [01006F8002326000][US][v0].nsp"
run_optional_case nds-retroarch retroarch "$(exe retroarch)" 16 -L "$(core desmume_libretro.so)" "$ROMS/nds/Castlevania - Dawn of Sorrow (USA).zip"

echo DONE | tee -a "$OUT/summary.tsv"
if [ "$FAILURES" -gt 0 ]; then
  exit 1
fi
