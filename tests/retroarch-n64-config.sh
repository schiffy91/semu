#!/usr/bin/env bash
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
SEMU_BIN="${SEMU_BIN:-$ROOT/build/out/semu}"

if ! command -v jq >/dev/null 2>&1; then
  printf '%s\n' "jq is required for RetroArch N64 validation" >&2
  exit 127
fi

failed=0

fail() {
  printf '%s\n' "RetroArch N64 config: $*" >&2
  failed=1
}

require_grep() {
  local needle="$1"
  local file="$2"
  if ! grep -F -- "$needle" "$file" >/dev/null; then
    fail "$file is missing: $needle"
  fi
}

require_jq() {
  local query="$1"
  local file="$2"
  if ! jq -e "$query" "$file" >/dev/null; then
    fail "$file failed jq contract: $query"
  fi
}

if [ ! -x "$SEMU_BIN" ]; then
  printf '%s\n' "SEMU_BIN is not executable: $SEMU_BIN" >&2
  exit 127
fi

state="$ROOT/config/emulators/retroarch/state.json"
template="$ROOT/config/emulators/retroarch/templates/Mupen64Plus-Next.opt"

require_jq '.templates[] | select(.source == "Mupen64Plus-Next.opt")' "$state"
require_jq '.directories[] | select(. == "emulators/retroarch/xdg/config/retroarch/config/Mupen64Plus-Next")' "$state"
require_grep 'mupen64plus-rdp-plugin = "angrylion"' "$template"
require_grep 'mupen64plus-rsp-plugin = "parallel"' "$template"
require_grep 'mupen64plus-angrylion-multithread = "all threads"' "$template"

tmp="$(mktemp -d "${TMPDIR:-/tmp}/semu-retroarch-n64.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT

project="$tmp/project"
home="$tmp/home"
mkdir -p "$project" "$home"
project="$(CDPATH= cd -- "$project" && pwd -P)"
home="$(CDPATH= cd -- "$home" && pwd -P)"

env HOME="$home" SEMU_ASSET_ROOT="$ROOT" SEMU_PROJECT_DIR="$project" \
  "$SEMU_BIN" build configs --project "$project" > "$tmp/build.log"

generated="$project/.semu/generated/emulators/retroarch/xdg/config/retroarch/config/Mupen64Plus-Next/Mupen64Plus-Next.opt"
require_grep 'mupen64plus-rdp-plugin = "angrylion"' "$generated"
require_grep 'mupen64plus-rsp-plugin = "parallel"' "$generated"
require_grep 'mupen64plus-angrylion-multithread = "all threads"' "$generated"

if [ "$failed" -ne 0 ]; then
  exit 1
fi

printf '%s\n' "OK RetroArch N64 core options"
