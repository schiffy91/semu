#!/usr/bin/env bash
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
SEMU_BIN="${SEMU_BIN:-$ROOT/build/out/semu}"

if ! command -v jq >/dev/null 2>&1; then
  printf '%s\n' "jq is required for Cemu/Ryujinx input validation" >&2
  exit 127
fi

if [ ! -x "$SEMU_BIN" ]; then
  printf '%s\n' "SEMU_BIN is not executable: $SEMU_BIN" >&2
  exit 127
fi

failed=0

fail() {
  printf '%s\n' "Cemu/Ryujinx input: $*" >&2
  failed=1
}

require_grep() {
  local needle="$1"
  local file="$2"
  if ! grep -F -- "$needle" "$file" >/dev/null; then
    fail "$file is missing: $needle"
  fi
}

reject_grep() {
  local needle="$1"
  local file="$2"
  if grep -F -- "$needle" "$file" >/dev/null; then
    fail "$file must not contain: $needle"
  fi
}

require_jq() {
  local query="$1"
  local file="$2"
  if ! jq -e "$query" "$file" >/dev/null; then
    fail "$file failed jq contract: $query"
  fi
}

cemu_input="$ROOT/config/emulators/cemu/input.json"
cemu_state="$ROOT/config/emulators/cemu/state.json"
cemu_templates=(
  "$ROOT/config/emulators/cemu/templates/controller0.xml"
  "$ROOT/config/emulators/cemu/templates/SteamInput-P1.xml"
)
ryujinx_input="$ROOT/config/emulators/ryujinx/input.json"
ryujinx_state="$ROOT/config/emulators/ryujinx/state.json"
ryujinx_config="$ROOT/config/emulators/ryujinx/templates/Config.json"
deck_sdl_guid="0300f617de2800000512000000036800"
deck_cemu_uuid="0_0300f617de2800000512000000036800"
deck_ryujinx_id="0-00000003-28de-0000-0512-000000036800"
ryujinx_profile="$ROOT/config/emulators/ryujinx/templates/Steam Deck Controller.json"

for file in "$cemu_input" "$cemu_state" "$ryujinx_input" "$ryujinx_state" "$ryujinx_config" "$ryujinx_profile"; do
  if ! jq -e . "$file" >/dev/null; then
    fail "$file is not valid JSON"
  fi
done

require_jq ".device.cemuUuid == \"$deck_cemu_uuid\"" "$cemu_input"
require_jq ".device.sdlGuid == \"$deck_sdl_guid\"" "$cemu_input"
require_jq '.templates[] | select(.source == "controller0.xml")' "$cemu_state"
require_jq '.templates[] | select(.source == "SteamInput-P1.xml")' "$cemu_state"

for file in "${cemu_templates[@]}"; do
  require_grep '<api>DSUController</api>' "$file"
  require_grep '<motion>true</motion>' "$file"
  require_grep '<api>SDLController</api>' "$file"
  require_grep "<uuid>$deck_cemu_uuid</uuid>" "$file"
  require_grep '<display_name>Steam Deck Controller</display_name>' "$file"
  require_grep '<mapping>24</mapping><button>40</button>' "$file"
  reject_grep '0_030079f6de280000ff11000001000000' "$file"
done

require_jq '.device.name == "Steam Deck Controller"' "$ryujinx_input"
require_jq '.device.backend == "GamepadSDL2"' "$ryujinx_input"
require_jq ".device.ryujinxId == \"$deck_ryujinx_id\"" "$ryujinx_input"
require_jq '.templates[] | select(.source == "Steam Deck Controller.json")' "$ryujinx_state"
require_jq '.input_config[0].backend == "GamepadSDL2"' "$ryujinx_config"
require_jq ".input_config[0].id == \"$deck_ryujinx_id\"" "$ryujinx_config"
require_jq '.input_config[0].name == "Steam Deck Controller"' "$ryujinx_config"
require_jq '.input_config[0].controller_type == "ProController"' "$ryujinx_config"
require_jq '.input_config[0].player_index == "Player1"' "$ryujinx_config"
require_jq '.input_config[0].led.enable_led == false' "$ryujinx_config"
require_jq '.input_config[0].left_joycon_stick.rotate90_cw == false' "$ryujinx_config"
require_jq '.input_config[0].right_joycon_stick.rotate90_cw == false' "$ryujinx_config"
require_jq '.input_config[0].motion.dsu_server_host == null' "$ryujinx_config"
require_jq '.input_config[0].motion.dsu_server_port == 0' "$ryujinx_config"
require_jq '.input_config[0].deadzone_left == 0.1' "$ryujinx_config"
require_jq '.input_config[0].deadzone_right == 0.1' "$ryujinx_config"
require_jq '.input_config[0].range_left == 1' "$ryujinx_config"
require_jq '.input_config[0].range_right == 1' "$ryujinx_config"
require_jq '.input_config[0].trigger_threshold == 0.5' "$ryujinx_config"
require_jq '.name == "Steam Deck Controller"' "$ryujinx_profile"
require_jq ".id == \"$deck_ryujinx_id\"" "$ryujinx_profile"
require_jq '.led.enable_led == false' "$ryujinx_profile"
require_jq '.left_joycon_stick.rotate90_cw == false' "$ryujinx_profile"
require_jq '.right_joycon_stick.rotate90_cw == false' "$ryujinx_profile"
require_jq '.motion.dsu_server_host == null' "$ryujinx_profile"
require_jq '.motion.dsu_server_port == 0' "$ryujinx_profile"
require_jq '.deadzone_left == 0.1' "$ryujinx_profile"
require_jq '.deadzone_right == 0.1' "$ryujinx_profile"
require_jq '.range_left == 1' "$ryujinx_profile"
require_jq '.range_right == 1' "$ryujinx_profile"
require_jq '.trigger_threshold == 0.5' "$ryujinx_profile"

if find "$ROOT/config/emulators/ryujinx" -type f \( -name '*Steam Virtual Controller*' -o -name '*Steam Virtual Gamepad*' \) | grep . >/dev/null; then
  fail "obsolete Ryujinx Steam virtual profile still exists"
fi

tmp="$(mktemp -d "${TMPDIR:-/tmp}/semu-cemu-ryujinx-input.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT

project="$tmp/project"
home="$tmp/home"
mkdir -p "$project" "$home"
project="$(CDPATH= cd -- "$project" && pwd -P)"
home="$(CDPATH= cd -- "$home" && pwd -P)"

env HOME="$home" SEMU_ASSET_ROOT="$ROOT" SEMU_PROJECT_DIR="$project" \
  "$SEMU_BIN" build configs --project "$project" > "$tmp/build.log"

generated="$project/.semu/generated"
generated_cemu="$generated/emulators/cemu/xdg/config/Cemu/controllerProfiles/controller0.xml"
generated_ryujinx_config="$generated/emulators/ryujinx/home/.config/Ryujinx/Config.json"
generated_ryujinx_profile="$generated/emulators/ryujinx/home/.config/Ryujinx/profiles/controller/Steam Deck Controller.json"

require_grep '<api>DSUController</api>' "$generated_cemu"
require_grep "<uuid>$deck_cemu_uuid</uuid>" "$generated_cemu"
require_jq '.input_config[0].name == "Steam Deck Controller"' "$generated_ryujinx_config"
require_jq ".input_config[0].id == \"$deck_ryujinx_id\"" "$generated_ryujinx_config"
require_jq '.input_config[0].led.enable_led == false' "$generated_ryujinx_config"
require_jq '.name == "Steam Deck Controller"' "$generated_ryujinx_profile"
require_jq '.led.enable_led == false' "$generated_ryujinx_profile"

if [ "$failed" -ne 0 ]; then
  exit 1
fi

printf '%s\n' "OK Cemu/Ryujinx Steam Deck input configs"
