#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd -P)"

if [ "$(uname -s)" != "Linux" ]; then
  echo "SKIP quit-watch smoke requires Linux input_event ABI"
  exit 0
fi

TMP="${TMPDIR:-/tmp}/semu-quit-watch-smoke.$$"
INPUT_DIR="$TMP/input"
LOG="$TMP/quit-watch.log"
PIDFILE="$TMP/grandchild.pid"
WATCH_PID=""
KEEPER_PID=""
GRANDCHILD_PID=""

cleanup() {
  set +e
  if [ -n "$WATCH_PID" ]; then
    kill "$WATCH_PID" 2>/dev/null || true
    kill -9 "$WATCH_PID" 2>/dev/null || true
  fi
  if [ -n "$KEEPER_PID" ]; then
    kill "$KEEPER_PID" 2>/dev/null || true
    kill -9 "$KEEPER_PID" 2>/dev/null || true
  fi
  if [ -n "$GRANDCHILD_PID" ]; then
    kill "$GRANDCHILD_PID" 2>/dev/null || true
    kill -9 "$GRANDCHILD_PID" 2>/dev/null || true
  fi
  rm -rf "$TMP"
}
trap cleanup EXIT

mkdir -p "$INPUT_DIR"
mkfifo "$INPUT_DIR/event0"

build_watch() {
  if [ -n "${SEMU_QUIT_WATCH_BIN:-}" ] && [ -x "$SEMU_QUIT_WATCH_BIN" ]; then
    printf '%s\n' "$SEMU_QUIT_WATCH_BIN"
    return 0
  fi
  if [ -x "$REPO_ROOT/build/out/semu-quit-watch" ]; then
    printf '%s\n' "$REPO_ROOT/build/out/semu-quit-watch"
    return 0
  fi
  if command -v semu-quit-watch >/dev/null 2>&1; then
    command -v semu-quit-watch
    return 0
  fi
  if command -v cc >/dev/null 2>&1; then
    cc -D_DEFAULT_SOURCE -x c "$REPO_ROOT/src/lib/quit_watch.btrc" -std=c11 -O2 -Wall -Wextra -o "$TMP/semu-quit-watch"
    printf '%s\n' "$TMP/semu-quit-watch"
    return 0
  fi
  echo "SKIP no semu-quit-watch binary or cc available" >&2
  return 77
}

build_emitter() {
  if ! command -v cc >/dev/null 2>&1; then
    echo "SKIP no cc available for synthetic input emitter" >&2
    return 77
  fi
  cat > "$TMP/emit-select-start.c" <<'C'
#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int emit_event(int fd, int type, int code, int value) {
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    return write(fd, &event, sizeof(event)) == sizeof(event) ? 0 : -1;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: emit-select-start EVENT_FIFO\n");
        return 64;
    }
    int fd = open(argv[1], O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    if (emit_event(fd, EV_KEY, BTN_SELECT, 1) != 0 ||
        emit_event(fd, EV_KEY, BTN_START, 1) != 0 ||
        emit_event(fd, EV_SYN, SYN_REPORT, 0) != 0 ||
        emit_event(fd, EV_KEY, BTN_START, 0) != 0 ||
        emit_event(fd, EV_KEY, BTN_SELECT, 0) != 0 ||
        emit_event(fd, EV_SYN, SYN_REPORT, 0) != 0) {
        fprintf(stderr, "write input_event: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}
C
  cc "$TMP/emit-select-start.c" -std=c11 -O2 -Wall -Wextra -o "$TMP/emit-select-start"
  printf '%s\n' "$TMP/emit-select-start"
}

wait_until() {
  local attempts="$1"
  shift
  local i=0
  while [ "$i" -lt "$attempts" ]; do
    if "$@"; then
      return 0
    fi
    sleep 0.1
    i=$((i + 1))
  done
  return 1
}

file_has_watch() {
  grep -q ' watch$' "$LOG" 2>/dev/null
}

file_has_pid() {
  [ -s "$PIDFILE" ]
}

process_gone() {
  ! kill -0 "$GRANDCHILD_PID" 2>/dev/null
}

WATCH="$(build_watch)" || {
  status="$?"
  [ "$status" = "77" ] && exit 0
  exit "$status"
}
EMIT="$(build_emitter)" || {
  status="$?"
  [ "$status" = "77" ] && exit 0
  exit "$status"
}

( while :; do sleep 60; done ) > "$INPUT_DIR/event0" &
KEEPER_PID="$!"

SEMU_QUIT_WATCH_INPUT_DIR="$INPUT_DIR" \
SEMU_QUIT_WATCH_LOG="$LOG" \
"$WATCH" -- sh -c '
  trap "" TERM
  sh -c '"'"'
    trap "" TERM
    printf "%s\n" "$$" > "$1"
    while :; do sleep 1; done
  '"'"' semu-grandchild "$1" &
  wait
' semu-child "$PIDFILE" &
WATCH_PID="$!"

wait_until 50 file_has_pid || {
  echo "FAIL quit-watch child did not start"
  exit 1
}
GRANDCHILD_PID="$(cat "$PIDFILE")"

wait_until 50 file_has_watch || {
  echo "FAIL quit-watch did not open synthetic input"
  exit 1
}

"$EMIT" "$INPUT_DIR/event0"

wait_until 50 process_gone || {
  echo "FAIL Select+Start did not kill child process group"
  exit 1
}

set +e
wait "$WATCH_PID"
STATUS="$?"
WATCH_PID=""
set -e

if [ "$STATUS" != "0" ]; then
  echo "FAIL quit-watch exited with status $STATUS"
  exit 1
fi
if ! grep -q 'quit reason=select+start' "$LOG"; then
  echo "FAIL quit-watch did not record select+start quit"
  exit 1
fi
if ! grep -q 'terminate reason=select+start' "$LOG"; then
  echo "FAIL quit-watch did not record select+start termination"
  exit 1
fi

echo "OK quit-watch select+start kills launched process group"
