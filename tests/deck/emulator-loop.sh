#!/usr/bin/env bash
set -u

APP="${SEMU_APPIMAGE:-/home/deck/Applications/Semu/Semu-x86_64.AppImage}"
PROJECT="${SEMU_PROJECT:-/home/deck/semu}"
ROMS="${SEMU_ROMS:-/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs}"
RESULT="${SEMU_RESULT:-/home/deck/.cache/semu-codex-src/result}"
OUT="${SEMU_TEST_OUT:-/home/deck/.cache/semu-codex-emulator-loop}"
SEND="${SEMU_UINPUT_SEND:-/home/deck/.cache/semu-uinput-send}"

mkdir -p "$OUT"
: > "$OUT/summary.tsv"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/1000}"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/1000/bus}"
export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export DISPLAY="${DISPLAY:-:0}"
export XAUTHORITY="${XAUTHORITY:-/run/user/1000/xauth_WjTByH}"

ps -eo pid,args | awk '/\/usr\/bin\/es-de --no-splash/ {print $1}' | xargs -r kill 2>/dev/null || true
sleep 2

capture_screen() {
  local path="$1"
  spectacle -b -n -o "$path" >/tmp/semu-loop-spectacle.log 2>&1 || cat /tmp/semu-loop-spectacle.log >&2
}

alive() {
  kill -0 "$1" 2>/dev/null
}

run_case() {
  local id="$1"
  local emulator="$2"
  local executable="$3"
  local wait_seconds="$4"
  shift 4

  local log="$OUT/$id.log"
  local png="$OUT/$id.png"
  local status="unknown"
  local quit="unknown"
  local size="0"

  rm -f "$log" "$png"
  printf 'START\t%s\t%s\n' "$id" "$(date +%H:%M:%S)" | tee -a "$OUT/summary.tsv"

  setsid env \
    XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    DBUS_SESSION_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS" \
    WAYLAND_DISPLAY="$WAYLAND_DISPLAY" \
    DISPLAY="$DISPLAY" \
    XAUTHORITY="$XAUTHORITY" \
    SEMU_PROJECT_DIR="$PROJECT" \
    SEMU_ROMS="$ROMS" \
    SEMU_ROMS_DIR="$ROMS" \
    SEMU_QUIT_WATCH_DEBUG=1 \
    "$APP" launcher routed "$emulator" "$executable" "$@" >"$log" 2>&1 < /dev/null &

  local pid=$!
  sleep "$wait_seconds"
  capture_screen "$png"
  [ -f "$png" ] && size="$(wc -c < "$png" | tr -d ' ')"

  if alive "$pid"; then
    sudo "$SEND" select-start >/dev/null 2>&1 || true
    sleep 6
  fi
  if alive "$pid"; then
    sudo "$SEND" esc >/dev/null 2>&1 || true
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
  if grep -q 'quit key: select+start\|quit key: escape\|quit requested' "$log" 2>/dev/null; then
    quit="ok"
  else
    quit="missing"
  fi
  printf 'RESULT\t%s\tstatus=%s\tquit=%s\tpng_bytes=%s\n' "$id" "$status" "$quit" "$size" | tee -a "$OUT/summary.tsv"
  tail -20 "$log" > "$OUT/$id.tail" 2>/dev/null || true
  sleep 2
}

core() {
  readlink -f "$RESULT/lib/retroarch/cores/$1"
}

exe() {
  readlink -f "$RESULT/bin/$1"
}

run_case gb retroarch "$(exe retroarch)" 12 -L "$(core gambatte_libretro.so)" "$ROMS/gb/Tetris (World) (Rev 1).zip"
run_case gbc retroarch "$(exe retroarch)" 12 -L "$(core gambatte_libretro.so)" "$ROMS/gbc/Game & Watch Gallery 3 (USA, Europe) (SGB Enhanced) (GB Compatible).zip"
run_case gba retroarch "$(exe retroarch)" 12 -L "$(core mgba_libretro.so)" "$ROMS/gba/Mega Man Zero 3 (USA).zip"
run_case nes retroarch "$(exe retroarch)" 12 -L "$(core mesen_libretro.so)" "$ROMS/nes/Bionic Commando (USA).zip"
run_case snes retroarch "$(exe retroarch)" 12 -L "$(core snes9x_libretro.so)" "$ROMS/snes/Super Metroid (Japan, USA) (En,Ja).zip"
run_case genesis retroarch "$(exe retroarch)" 12 -L "$(core genesis_plus_gx_libretro.so)" "$ROMS/genesis/Sonic The Hedgehog (USA, Europe).zip"
run_case n64-retroarch retroarch "$(exe retroarch)" 16 -L "$(core mupen64plus_next_libretro.so)" "$ROMS/n64/Super Smash Bros. (USA).zip"
run_case nds-retroarch retroarch "$(exe retroarch)" 16 -L "$(core desmume_libretro.so)" "$ROMS/nds/Castlevania - Dawn of Sorrow (USA).zip"
run_case psp ppsspp "$(exe ppsspp)" 18 "$ROMS/psp/LocoRoco (USA) (En,Ja,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi,Zh,Ko,Ru).iso"
run_case dreamcast flycast "$(exe flycast)" 18 "$ROMS/dreamcast/ChuChu Rocket! (USA) (En,Ja,Fr,De,Es).chd"
run_case gc dolphin "$(exe dolphin-emu)" 22 "$ROMS/gc/Super Monkey Ball 2 (USA).rvz"
run_case wii dolphin "$(exe dolphin-emu)" 24 "$ROMS/wii/Kirby's Epic Yarn (USA) (En,Fr,Es).wbfs"
run_case ps2 pcsx2 "$(exe pcsx2-qt)" 28 "$ROMS/ps2/Devil May Cry (USA).iso"
run_case nds-melonds melonds "$(exe melonDS)" 18 "$ROMS/nds/Castlevania - Dawn of Sorrow (USA).zip"
run_case n3ds azahar "$(exe azahar)" 28 "$ROMS/n3ds/Super Mario 3D Land (USA) (En,Fr,Es) (Rev 1).3ds"
run_case wiiu cemu "$(exe Cemu)" 30 "$ROMS/wiiu/New SUPER MARIO BROS. U (US).wua"
run_case switch ryujinx "$(exe Ryujinx)" 35 "$ROMS/switch/Animal Crossing New Horizons [01006F8002326000][US][v0].nsp"

echo DONE | tee -a "$OUT/summary.tsv"
