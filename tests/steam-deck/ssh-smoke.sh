#!/usr/bin/env bash
set -Eeuo pipefail

DECK_HOST="${DECK_HOST:-steamdeck.local}"
DECK_USER="${DECK_USER:-deck}"
DECK_TARGET="${DECK_USER}@${DECK_HOST}"
REMOTE_ROOT="${DECK_REMOTE_ROOT:-/home/deck/semu-smoke}"
REMOTE_APP_DIR="${DECK_APP_DIR:-/home/deck/Applications/Semu}"
ROMS_HINT="${DECK_ROMS:-/mnt/SD}"
LAUNCH_WAIT="${DECK_LAUNCH_WAIT:-8}"
LOCAL_OUT="${DECK_LOCAL_OUT:-$PWD/.deck-smoke/$DECK_HOST}"

usage() {
  cat <<'EOF'
Usage:
  tests/steam-deck/ssh-smoke.sh [Semu-or-ES-DE.AppImage ...]

Environment:
  DECK_HOST=steamdeck.local     SSH host
  DECK_USER=deck                SSH user
  DECK_ROMS=/mnt/SD             ROM mount hint; checked read-only only
  DECK_LAUNCH_WAIT=8            Seconds to wait before screenshot
  DECK_LOCAL_OUT=.deck-smoke/... Local screenshot/log output

The script writes only under ~/semu-smoke and ~/Applications/Semu on the Deck.
It does not create, delete, scan, or modify ROM files.
EOF
}

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
  usage
  exit 0
fi

ssh_deck() {
  ssh -o BatchMode=yes -o ConnectTimeout=10 "$DECK_TARGET" "$@"
}

remote_quote() {
  printf "%q" "$1"
}

mkdir -p "$LOCAL_OUT"

echo "== Probe $DECK_TARGET =="
ssh_deck "REMOTE_ROOT=$(remote_quote "$REMOTE_ROOT") REMOTE_APP_DIR=$(remote_quote "$REMOTE_APP_DIR") ROMS_HINT=$(remote_quote "$ROMS_HINT") bash -s" <<'REMOTE'
set -Eeuo pipefail
mkdir -p "$REMOTE_ROOT/screenshots" "$REMOTE_ROOT/logs" "$REMOTE_APP_DIR"
{
  printf 'user=%s\n' "$USER"
  printf 'home=%s\n' "$HOME"
  . /etc/os-release 2>/dev/null || true
  printf 'os=%s %s\n' "${NAME:-unknown}" "${VERSION_ID:-unknown}"
  printf 'sudo_noninteractive='
  if sudo -n true 2>/dev/null; then echo yes; else echo no; fi
  printf 'readonly='
  steamos-readonly status 2>/dev/null || echo unknown
  printf 'roms_hint='
  if [ -d "$ROMS_HINT" ]; then
    printf '%s exists\n' "$ROMS_HINT"
  elif [ -d /run/media/deck/SD ]; then
    printf '%s missing; /run/media/deck/SD exists\n' "$ROMS_HINT"
  elif [ -e "$ROMS_HINT" ]; then
    printf '%s exists but is not a directory\n' "$ROMS_HINT"
  else
    printf '%s missing\n' "$ROMS_HINT"
  fi
  printf 'fuse='
  if [ -e /dev/fuse ]; then echo yes; else echo no; fi
  printf 'session_env:\n'
  systemctl --user show-environment 2>/dev/null \
    | grep -E '^(DISPLAY|WAYLAND_DISPLAY|XDG_CURRENT_DESKTOP|XDG_SESSION_TYPE|DBUS_SESSION_BUS_ADDRESS)=' \
    || true
  printf 'screenshot_tool='
  command -v spectacle || command -v grim || command -v gnome-screenshot || command -v import || true
} | tee "$REMOTE_ROOT/probe.txt"
REMOTE

if [ "$#" -gt 0 ]; then
  echo "== Install AppImages to $REMOTE_APP_DIR =="
fi

for app in "$@"; do
  if [ ! -f "$app" ]; then
    echo "missing AppImage: $app" >&2
    exit 2
  fi
  case "$app" in
    *.AppImage|*.appimage) ;;
    *) echo "not an AppImage path: $app" >&2; exit 2 ;;
  esac
  scp -q -- "$app" "$DECK_TARGET:$REMOTE_APP_DIR/"
  base="$(basename "$app")"
  ssh_deck "chmod +x $(remote_quote "$REMOTE_APP_DIR/$base")"
done

echo "== Capture baseline screenshot =="
ssh_deck "REMOTE_ROOT=$(remote_quote "$REMOTE_ROOT") bash -s" <<'REMOTE'
set -Eeuo pipefail
mkdir -p "$REMOTE_ROOT/screenshots" "$REMOTE_ROOT/logs"
if systemctl --user show-environment > "$REMOTE_ROOT/user-env" 2>/dev/null; then
  set -a
  # shellcheck disable=SC1090
  . "$REMOTE_ROOT/user-env"
  set +a
fi
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export LANG="${LANG:-en_US.UTF-8}"
export LC_ALL="${LC_ALL:-en_US.UTF-8}"
out="$REMOTE_ROOT/screenshots/baseline-$(date +%Y%m%d-%H%M%S).png"
if command -v spectacle >/dev/null 2>&1; then
  spectacle -b -n -f -o "$out" >"$REMOTE_ROOT/logs/baseline-screenshot.log" 2>&1
elif command -v grim >/dev/null 2>&1; then
  grim "$out" >"$REMOTE_ROOT/logs/baseline-screenshot.log" 2>&1
elif command -v gnome-screenshot >/dev/null 2>&1; then
  gnome-screenshot -f "$out" >"$REMOTE_ROOT/logs/baseline-screenshot.log" 2>&1
elif command -v import >/dev/null 2>&1; then
  import -window root "$out" >"$REMOTE_ROOT/logs/baseline-screenshot.log" 2>&1
else
  echo "no screenshot tool found" >&2
  exit 3
fi
if [ ! -s "$out" ]; then
  echo "screenshot command did not produce $out" >&2
  cat "$REMOTE_ROOT/logs/baseline-screenshot.log" >&2 || true
  journalctl --user --since "2 minutes ago" --no-pager 2>/dev/null \
    | grep -Ei 'spectacle|screenshot|portal|kwin' >&2 || true
  exit 4
fi
file "$out" 2>/dev/null || ls -l "$out"
REMOTE

if [ "$#" -gt 0 ]; then
  echo "== Launch AppImages and screenshot =="
fi

for app in "$@"; do
  base="$(basename "$app")"
  ssh_deck "REMOTE_ROOT=$(remote_quote "$REMOTE_ROOT") APP=$(remote_quote "$REMOTE_APP_DIR/$base") LAUNCH_WAIT=$(remote_quote "$LAUNCH_WAIT") bash -s" <<'REMOTE'
set -Eeuo pipefail
mkdir -p "$REMOTE_ROOT/screenshots" "$REMOTE_ROOT/logs"
if systemctl --user show-environment > "$REMOTE_ROOT/user-env" 2>/dev/null; then
  set -a
  # shellcheck disable=SC1090
  . "$REMOTE_ROOT/user-env"
  set +a
fi
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export LANG="${LANG:-en_US.UTF-8}"
export LC_ALL="${LC_ALL:-en_US.UTF-8}"
label="$(basename "$APP" | tr -c 'A-Za-z0-9._-' '_')"
log="$REMOTE_ROOT/logs/$label.log"
setsid "$APP" >"$log" 2>&1 &
pid="$!"
sleep "$LAUNCH_WAIT"
out="$REMOTE_ROOT/screenshots/$label-$(date +%Y%m%d-%H%M%S).png"
if command -v spectacle >/dev/null 2>&1; then
  spectacle -b -n -f -o "$out" >>"$log" 2>&1 || true
elif command -v grim >/dev/null 2>&1; then
  grim "$out" >>"$log" 2>&1 || true
elif command -v gnome-screenshot >/dev/null 2>&1; then
  gnome-screenshot -f "$out" >>"$log" 2>&1 || true
elif command -v import >/dev/null 2>&1; then
  import -window root "$out" >>"$log" 2>&1 || true
fi
kill -TERM "-$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true
sleep 1
kill -KILL "-$pid" 2>/dev/null || kill -KILL "$pid" 2>/dev/null || true
if [ ! -s "$out" ]; then
  echo "screenshot missing for $APP" >&2
  tail -80 "$log" >&2 || true
  journalctl --user --since "2 minutes ago" --no-pager 2>/dev/null \
    | grep -Ei 'spectacle|screenshot|portal|kwin' >&2 || true
  exit 4
fi
file "$out" 2>/dev/null || ls -l "$out"
REMOTE
done

echo "== Pull smoke artifacts to $LOCAL_OUT =="
scp -q "$DECK_TARGET:$REMOTE_ROOT/probe.txt" "$LOCAL_OUT/" || true
scp -q "$DECK_TARGET:$REMOTE_ROOT/screenshots/"'*.png' "$LOCAL_OUT/" || true
scp -q "$DECK_TARGET:$REMOTE_ROOT/logs/"'*.log' "$LOCAL_OUT/" || true

echo "== Local screenshot files =="
find "$LOCAL_OUT" -maxdepth 1 -type f -name '*.png' -print0 \
  | xargs -0 -r file

echo "OK deck ssh smoke artifacts: $LOCAL_OUT"
