#!/usr/bin/env bash
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"

if ! command -v jq >/dev/null 2>&1; then
  printf '%s\n' "jq is required for rendering contract validation" >&2
  exit 127
fi

failed=0

fail() {
  printf '%s\n' "rendering contract: $*" >&2
  failed=1
}

contains() {
  local needle="$1"
  shift
  local item
  for item in "$@"; do
    if [ "$item" = "$needle" ]; then
      return 0
    fi
  done
  return 1
}

json_string() {
  local file="$1"
  local query="$2"
  jq -r "$query // empty" "$file"
}

check_file() {
  local label="$1"
  local path="$2"

  if [ ! -f "$path" ]; then
    fail "$label references missing repo-owned file: ${path#$ROOT/}"
  fi
}

check_non_placeholder_value() {
  local system="$1"
  local field="$2"
  local value="$3"

  case "$(printf '%s' "$value" | tr '[:upper:]' '[:lower:]')" in
    ""|"null"|"none"|*"placeholder"*|*"dummy"*|*"todo"*|*"tbd"*|*"noop"*|*"no-op"*|*"border-only"*|*"functional-content"*)
      fail "$system renderer.$field must not be a placeholder/no-op declaration: ${value:-<missing>}"
      ;;
  esac
}

check_effect_is_substantive() {
  local system="$1"
  local field="$2"
  local path="$3"
  local kind="$4"
  local rel="${path#$ROOT/}"

  if [ ! -f "$path" ]; then
    return
  fi

  if grep -Eiq 'placeholder|dummy|todo|tbd|noop|no-op|pass[-_ ]?through|sample[-_ ]?only' "$path"; then
    fail "$system renderer.$field effect must not identify itself as placeholder/no-op: $rel"
  fi

  case "$kind" in
    shader)
      if ! grep -Eq 'sin|smoothstep|pow|dot|fract|lerp|mix|scan|mask|grid|luma|vignette|curv' "$path"; then
        fail "$system renderer.$field effect must perform a visible shader transform: $rel"
      fi
      ;;
    bezel)
      if ! grep -Eq 'semuFrameMask|semuContentMask|semuRectMask|shell|cabinet|frame|bezel|hinge|button|dpad|speaker' "$path"; then
        fail "$system renderer.$field effect must declare visible bezel/frame composition: $rel"
      fi
      ;;
  esac
}

check_required_effect() {
  local system="$1"
  local field="$2"
  local file_name="$3"
  local effect_root="$4"
  local kind="${5:-shader}"

  if [ -z "$file_name" ] || [ "$file_name" = "null" ]; then
    fail "$system renderer.$field must declare an effect file"
    return
  fi

  check_non_placeholder_value "$system" "$field.effect_file" "$file_name"
  check_file "$system renderer.$field" "$ROOT/$effect_root/$file_name"
  check_effect_is_substantive "$system" "$field" "$ROOT/$effect_root/$file_name" "$kind"
}

check_optional_effect() {
  local system="$1"
  local field="$2"
  local file_name="$3"
  local effect_root="$4"

  if [ -n "$file_name" ] && [ "$file_name" != "null" ]; then
    check_file "$system renderer.$field" "$ROOT/$effect_root/$file_name"
  fi
}

validate_classic_renderer_feature() {
  local system="$1"
  local system_file="$2"
  local feature="$3"
  local effect_root="$4"

  if ! jq -e --arg feature "$feature" '.renderer[$feature] | type == "object"' "$system_file" >/dev/null; then
    fail "$system must declare renderer.$feature"
    return
  fi

  if ! jq -e --arg feature "$feature" '.renderer[$feature].enabled_by_default == true' "$system_file" >/dev/null; then
    fail "$system renderer.$feature must be enabled by default"
  fi
  if ! jq -e --arg feature "$feature" '(.renderer[$feature].family // "") | length > 0 and . != "none"' "$system_file" >/dev/null; then
    fail "$system renderer.$feature.family must identify a concrete family"
  fi

  if [ "$feature" = "shader" ]; then
    if ! jq -e '.renderer.shader.source_asset | type == "string" and length > 0' "$system_file" >/dev/null; then
      fail "$system renderer.shader.source_asset must identify the intended shader/effect source"
    else
      check_non_placeholder_value "$system" "shader.source_asset" "$(json_string "$system_file" '.renderer.shader.source_asset')"
    fi
    check_required_effect "$system" "shader" "$(json_string "$system_file" '.renderer.shader.effect_file')" "$effect_root" "shader"
  else
    if ! jq -e '(.renderer.bezel.source_asset // "") | length > 0' "$system_file" >/dev/null; then
      fail "$system renderer.bezel.source_asset must identify a bezel asset"
    else
      check_non_placeholder_value "$system" "bezel.source_asset" "$(json_string "$system_file" '.renderer.bezel.source_asset')"
    fi
    check_required_effect "$system" "bezel" "$(json_string "$system_file" '.renderer.bezel.composition_effect_file')" "$effect_root" "bezel"
  fi
}

validate_modern_renderer_feature() {
  local system="$1"
  local system_file="$2"
  local feature="$3"
  local effect_root="$4"

  if ! jq -e --arg feature "$feature" '.renderer[$feature] | type == "object"' "$system_file" >/dev/null; then
    fail "$system must declare renderer.$feature"
    return
  fi

  if ! jq -e --arg feature "$feature" '.renderer[$feature] | has("enabled_by_default")' "$system_file" >/dev/null; then
    fail "$system renderer.$feature must explicitly declare enabled_by_default"
  fi
  if ! jq -e --arg feature "$feature" '(.renderer[$feature].family // "") | length > 0' "$system_file" >/dev/null; then
    fail "$system renderer.$feature.family must be explicit even when disabled"
  fi

  if [ "$feature" = "shader" ]; then
    check_optional_effect "$system" "shader" "$(json_string "$system_file" '.renderer.shader.effect_file')" "$effect_root"
  else
    check_optional_effect "$system" "bezel" "$(json_string "$system_file" '.renderer.bezel.composition_effect_file')" "$effect_root"
  fi
}

validate_implemented_emulator() {
  local system="$1"
  local emulator="$2"
  local emulator_file="$3"
  local status="$4"
  local shader_enabled="$5"
  local bezel_enabled="$6"

  if ! jq -e '.semu_renderer.source_hook_enabled == true' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' status=$status must enable the Semu hook"
  fi
  if ! jq -e '.semu_renderer.preferred_backend == "source_hook"' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' status=$status must prefer source_hook"
  fi
  if ! jq -e '.capabilities.generic_renderer == true' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' status=$status must expose generic_renderer capability"
  fi
  if ! jq -e '.capabilities.native_shaders == false' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' must not rely on emulator-native shader architecture"
  fi
  if ! jq -e '.capabilities.native_bezels == false' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' must not rely on emulator-native bezel architecture"
  fi
  if ! jq -e '.semu_renderer.hook_excludes_ui == true' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' must declare hook_excludes_ui=true"
  fi
  if ! jq -e '(.semu_renderer.hook_scope // "") | IN("game_framebuffer", "content_video_driver_present")' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' must hook game framebuffer/content present scope"
  fi
  if ! jq -e '(.semu_renderer.hook_point // "") | length > 0' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' must declare hook_point"
  fi
  if [ "$shader_enabled" = "true" ] && ! jq -e '.semu_renderer.receives_shader_assets == true' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' must receive shader assets"
  fi
  if [ "$bezel_enabled" = "true" ] && ! jq -e '.semu_renderer.receives_bezel_assets == true' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' must receive bezel assets"
  fi

  if [ "$status" != "implemented_source_hook" ]; then
    fail "$system emulator '$emulator' must be implemented_source_hook for production Deck coverage, got $status"
  elif ! jq -e '.semu_renderer.hook_scope == "game_framebuffer"' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' implemented_source_hook must use game_framebuffer"
  fi
}

validate_planned_emulator() {
  local system="$1"
  local emulator="$2"
  local emulator_file="$3"
  local shader_enabled="$4"
  local bezel_enabled="$5"

  if ! jq -e '.semu_renderer.source_hook_enabled == false' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_source_hook must not enable the hook yet"
  fi
  if ! jq -e '.semu_renderer.preferred_backend == "native_fullscreen_only"' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_source_hook must use native_fullscreen_only as its current backend"
  fi
  if ! jq -e '.capabilities.generic_renderer == false' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_source_hook must not claim generic_renderer capability"
  fi
  if ! jq -e '.semu_renderer.hook_excludes_ui == true' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_source_hook must still declare hook_excludes_ui=true"
  fi
  if ! jq -e '.semu_renderer.hook_requires_source_package == true' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_source_hook must require a source package"
  fi
  if ! jq -e '.semu_renderer.source_package_status == "source-package-pending"' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_source_hook must declare source-package-pending"
  fi
  if ! jq -e '(.semu_renderer.required_hook_scope // "") | IN("game_framebuffer", "content_video_driver_present")' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_source_hook must declare required_hook_scope"
  fi
  if ! jq -e '(.semu_renderer.required_hook_point // "") | length > 0' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_source_hook must declare required_hook_point"
  fi
  if ! jq -e '.semu_renderer.planned_hook_scope == .semu_renderer.required_hook_scope' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_hook_scope must match required_hook_scope"
  fi
  if ! jq -e '.semu_renderer.planned_hook_point == .semu_renderer.required_hook_point' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_hook_point must match required_hook_point"
  fi
  if jq -e '.semu_renderer | has("hook_scope") or has("hook_point") or has("hook_contract")' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' planned_source_hook must not declare implemented hook fields"
  fi
  if [ "$shader_enabled" = "true" ] || [ "$bezel_enabled" = "true" ]; then
    fail "$system emulator '$emulator' is only planned but the system enables default renderer assets"
  fi
}

validate_emulator_for_system() {
  local system="$1"
  local system_file="$2"
  local emulator="$3"
  local emulator_file="$4"
  local shader_enabled="$5"
  local bezel_enabled="$6"

  local status
  status="$(json_string "$emulator_file" '.semu_renderer.implementation_status')"

  case "$status" in
    implemented_source_hook)
      validate_implemented_emulator "$system" "$emulator" "$emulator_file" "$status" "$shader_enabled" "$bezel_enabled"
      ;;
    implemented_proof_hook)
      fail "$system emulator '$emulator' is proof-only; production targets require implemented_source_hook"
      ;;
    planned_source_hook)
      validate_planned_emulator "$system" "$emulator" "$emulator_file" "$shader_enabled" "$bezel_enabled"
      ;;
    not_required)
      fail "$system emulator '$emulator' serves a current system but is marked not_required"
      ;;
    "")
      fail "$system emulator '$emulator' must declare semu_renderer.implementation_status"
      ;;
    *)
      fail "$system emulator '$emulator' has unknown implementation_status: $status"
      ;;
  esac

  if ! jq -e '.semu_renderer.fallback_backend == "native_fullscreen_only"' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' must declare native_fullscreen_only fallback_backend"
  fi
  if ! jq -e '.generated_config_contract.owns_renderer_assets == false' "$emulator_file" >/dev/null; then
    fail "$system emulator '$emulator' must not claim generated config ownership of renderer assets"
  fi
}

defaults_file="$ROOT/config/assets/defaults.json"
effect_root="$(json_string "$defaults_file" '.renderer.effect_root')"
if [ -z "$effect_root" ]; then
  fail "config/assets/defaults.json must declare renderer.effect_root"
elif [ ! -d "$ROOT/$effect_root" ]; then
  fail "renderer.effect_root does not exist: $effect_root"
fi

classic_systems=()
modern_systems=()
current_systems=()

while IFS= read -r system; do
  [ -n "$system" ] || continue
  classic_systems+=("$system")
  current_systems+=("$system")
done < <(jq -r '.classic_systems[]?' "$defaults_file")

while IFS= read -r system; do
  [ -n "$system" ] || continue
  modern_systems+=("$system")
  current_systems+=("$system")
done < <(jq -r '.modern_systems[]?' "$defaults_file")

if [ "${#current_systems[@]}" -eq 0 ]; then
  fail "config/assets/defaults.json must list current systems"
fi

while IFS= read -r -d '' system_file; do
  system_from_path="${system_file##*/}"
  system_from_path="${system_from_path%.json}"
  if ! contains "$system_from_path" "${current_systems[@]}"; then
    fail "${system_file#$ROOT/} is not listed in classic_systems or modern_systems"
  fi
done < <(find "$ROOT/config/assets/systems" -type f -name '*.json' -print0)

for system in "${current_systems[@]}"; do
  system_file="$ROOT/config/assets/systems/$system.json"

  if [ ! -f "$system_file" ]; then
    fail "$system is missing config/assets/systems/$system.json"
    continue
  fi

  if ! jq -e . "$system_file" >/dev/null; then
    fail "$system_file is not valid JSON"
    continue
  fi

  declared_system="$(json_string "$system_file" '.system')"
  if [ "$declared_system" != "$system" ]; then
    fail "$system file must declare system=$system"
  fi

  renderer_model="$(json_string "$system_file" '.renderer.model')"
  if [ "$renderer_model" != "semu_source_hook_renderer" ]; then
    fail "$system renderer.model must be semu_source_hook_renderer, got '${renderer_model:-<missing>}'"
  fi

  if ! jq -e '.renderer.viewport_source == "content_viewport"' "$system_file" >/dev/null; then
    fail "$system renderer.viewport_source must target content_viewport"
  fi
  if ! jq -e '(.renderer.profile // "") | length > 0' "$system_file" >/dev/null; then
    fail "$system renderer.profile must identify the Semu renderer profile"
  fi
  if ! jq -e '.content_viewport | type == "object"' "$system_file" >/dev/null; then
    fail "$system must declare content_viewport"
  fi
  if ! jq -e '(.content_viewport.scale_policy // "") | length > 0' "$system_file" >/dev/null; then
    fail "$system content_viewport.scale_policy must be explicit"
  fi

  if contains "$system" "${classic_systems[@]}"; then
    validate_classic_renderer_feature "$system" "$system_file" "shader" "$effect_root"
    validate_classic_renderer_feature "$system" "$system_file" "bezel" "$effect_root"
  else
    validate_modern_renderer_feature "$system" "$system_file" "shader" "$effect_root"
    validate_modern_renderer_feature "$system" "$system_file" "bezel" "$effect_root"
  fi

  emulator="$(json_string "$system_file" '.emulator')"
  emulator_file="$ROOT/config/emulators/$emulator/rendering.json"
  if [ -z "$emulator" ]; then
    fail "$system must declare an emulator"
  elif [ ! -f "$emulator_file" ]; then
    fail "$system emulator '$emulator' is missing config/emulators/$emulator/rendering.json"
  else
    validate_emulator_for_system \
      "$system" \
      "$system_file" \
      "$emulator" \
      "$emulator_file" \
      "$(json_string "$system_file" '.renderer.shader.enabled_by_default')" \
      "$(json_string "$system_file" '.renderer.bezel.enabled_by_default')"
  fi
done

for emulator_file in "$ROOT"/config/emulators/*/rendering.json; do
  emulator="$(basename "$(dirname "$emulator_file")")"
  status="$(json_string "$emulator_file" '.semu_renderer.implementation_status')"
  case "$status" in
    implemented_source_hook|planned_source_hook|not_required) ;;
    implemented_proof_hook)
      fail "$emulator is proof-only; production targets require implemented_source_hook or not_required"
      ;;
    "")
      fail "$emulator must declare semu_renderer.implementation_status"
      ;;
    *)
      fail "$emulator has unknown implementation_status: $status"
      ;;
  esac

  if [ "$status" = "not_required" ]; then
    if ! jq -e '.semu_renderer.source_hook_enabled == false' "$emulator_file" >/dev/null; then
      fail "$emulator not_required must not enable a source hook"
    fi
    if ! jq -e '.capabilities.generic_renderer == false' "$emulator_file" >/dev/null; then
      fail "$emulator not_required must not expose generic_renderer"
    fi
  fi
done

while IFS= read -r -d '' file; do
  while IFS= read -r line; do
    include="${line#*\"}"
    include="${include%%\"*}"
    case "$include" in
      semu-*|./*|../*)
        check_file "${file#$ROOT/} include" "$(cd -- "$(dirname -- "$file")" && pwd -P)/$include"
        ;;
    esac
  done < <(grep -E '^[[:space:]]*#include[[:space:]]+"[^"]+"' "$file" || true)
done < <(find "$ROOT/assets/rendering" -type f \( -name '*.fx' -o -name '*.fxh' -o -name '*.slang' \) -print0)

while IFS= read -r -d '' preset; do
  while IFS= read -r shader; do
    shader="${shader#\"}"
    shader="${shader%\"}"
    case "$shader" in
      ""|/*|*://*) ;;
      *)
        check_file "${preset#$ROOT/} shader" "$(cd -- "$(dirname -- "$preset")" && pwd -P)/$shader"
        ;;
    esac
  done < <(awk -F= '/^[[:space:]]*shader[0-9]+[[:space:]]*=/ { gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2); print $2 }' "$preset")
done < <(find "$ROOT/assets/rendering" -type f -name '*.slangp' -print0)

forbidden_claims="$(
  while IFS= read -r -d '' file; do
    awk -v file="${file#$ROOT/}" '
      {
        line = $0
        l = tolower(line)
        has_backend = l ~ /(vkbasalt|gamescope|reshade|semu-render)/
        has_claim = l ~ /(production shader\/bezel path|production shader path|production bezel path|production renderer|production-ready|primary route|primary path|first generic backend|preferred[ _-]backend|default[ _-]backend|correct steam deck target)/
        allowed = l ~ /(fallback|prototype|experiment|not|must not|cannot|too fragile|fragile|deferred|research|removed|fails|crashes|not accepted|without .*proof|only after .*proof)/
        if (has_backend && has_claim && !allowed) {
          print file ":" FNR ": " line
        }
      }
    ' "$file"
  done < <(find "$ROOT/config" "$ROOT/docs" -type f \( -name '*.json' -o -name '*.md' -o -name '*.nix' -o -name '*.sh' \) -print0)
)"

if [ -n "$forbidden_claims" ]; then
  fail "vkBasalt/gamescope/ReShade/semu-render must not be described as the production shader/bezel path"
  printf '%s\n' "$forbidden_claims" >&2
fi

if [ "$failed" -ne 0 ]; then
  exit 1
fi

printf '%s\n' "OK rendering contract"
