#!/usr/bin/env bash
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"

if ! command -v jq >/dev/null 2>&1; then
  printf '%s\n' "jq is required for source-hook metadata validation" >&2
  exit 127
fi

failed=0

fail() {
  printf '%s\n' "source-hook metadata: $*" >&2
  failed=1
}

has_literal() {
  local needle="$1"
  local file="$2"
  grep -F -- "$needle" "$file" >/dev/null
}

for rendering in "$ROOT"/config/emulators/*/rendering.json; do
  id="$(basename "$(dirname "$rendering")")"
  package="$ROOT/config/emulators/$id/package.nix"

  if ! jq -e . "$rendering" >/dev/null; then
    fail "$id rendering.json is not valid JSON"
    continue
  fi

  if [ ! -f "$package" ]; then
    fail "$id is missing package.nix"
    continue
  fi

  enabled="$(jq -r '.semu_renderer.source_hook_enabled // false' "$rendering")"
  requires_source="$(jq -r '.semu_renderer.hook_requires_source_package // false' "$rendering")"
  preferred_backend="$(jq -r '.semu_renderer.preferred_backend // ""' "$rendering")"
  implementation_status="$(jq -r '.semu_renderer.implementation_status // ""' "$rendering")"
  generic_renderer="$(jq -r '.capabilities.generic_renderer // false' "$rendering")"
  package_status="$(jq -r '.semu_renderer.source_package_status // ""' "$rendering")"
  requires_deck_proof="$(jq -r '.semu_renderer.requires_deck_proof // false' "$rendering")"
  proof_env="$(jq -r '.semu_renderer.hook_contract.proof_env // ""' "$rendering")"
  config_env="$(jq -r '.semu_renderer.hook_contract.config_env // ""' "$rendering")"
  proof_marker="$(jq -r '.semu_renderer.hook_contract.proof_marker // ""' "$rendering")"
  native_shader_hook="$(jq -r 'if .semu_renderer.hook_contract | has("uses_emulator_native_shader_mechanism") then .semu_renderer.hook_contract.uses_emulator_native_shader_mechanism else empty end' "$rendering")"

  case "$package_status" in
    source-package-ready|source-package-pending|not-required) ;;
    "")
      fail "$id rendering.json must declare semu_renderer.source_package_status"
      ;;
    *)
      fail "$id has unknown source_package_status: $package_status"
      ;;
  esac

  if ! has_literal "sourceHook = {" "$package"; then
    fail "$id package.nix must declare sourceHook metadata"
  fi

  if ! has_literal "status = \"$package_status\";" "$package"; then
    fail "$id package.nix sourceHook status must match rendering.json ($package_status)"
  fi

  if [ "$enabled" = "true" ]; then
    if [ "$preferred_backend" != "source_hook" ]; then
      fail "$id source_hook_enabled=true must use preferred_backend=source_hook"
    fi

    if [ "$generic_renderer" != "true" ]; then
      fail "$id source_hook_enabled=true must expose generic_renderer capability"
    fi

    case "$package_status" in
      source-package-ready) ;;
      *)
        fail "$id source_hook_enabled=true cannot use package status $package_status"
        ;;
    esac

    if [ "$requires_source" = "true" ]; then
      if ! has_literal "mkSourceOverride" "$package"; then
        fail "$id requires a source package but package.nix has no mkSourceOverride path"
      fi
      if ! has_literal "productionPatchable = true;" "$package"; then
        fail "$id requires a source package but package metadata is not productionPatchable"
      fi
      if ! grep -E 'sourcePackagePath = "[^"]+";' "$package" >/dev/null; then
        fail "$id requires a source package but package metadata has no sourcePackagePath"
      fi

      if [ "$requires_source" = "true" ] && [ "$package_status" = "source-package-ready" ]; then
        if [ "$proof_env" != "SEMU_RENDER_HOOK_PROOF" ]; then
          fail "$id hook_contract.proof_env must be SEMU_RENDER_HOOK_PROOF"
        fi
        if [ "$config_env" != "SEMU_RENDER_HOOK_CONFIG" ]; then
          fail "$id hook_contract.config_env must be SEMU_RENDER_HOOK_CONFIG"
        fi
        if [ -z "$proof_marker" ]; then
          fail "$id hook_contract.proof_marker must be declared"
        elif ! has_literal "$proof_marker" "$package"; then
          fail "$id package.nix contractProof marker must match rendering.json"
        fi
        if [ "$native_shader_hook" != "false" ]; then
          fail "$id hook_contract must explicitly avoid emulator-native shader mechanisms"
        fi
        if ! has_literal "patches = semuPatches ++ patches;" "$package"; then
          fail "$id package.nix must wire Semu source-hook patches before caller patches"
        fi
        if ! has_literal "postPatchAssertions = semuPatchAssertions" "$package"; then
          fail "$id package.nix must wire Semu source-hook patch assertions"
        fi

        patch_count="$(jq -r '(.semu_renderer.hook_contract.patch_files // []) | length' "$rendering")"
        if [ "$patch_count" -eq 0 ]; then
          fail "$id hook_contract.patch_files must list at least one Semu patch"
        fi
        while IFS= read -r patch_file; do
          patch_path="$ROOT/$patch_file"
          if [ ! -f "$patch_path" ]; then
            fail "$id hook_contract references missing patch: $patch_file"
            continue
          fi
          if ! has_literal "SEMU_RENDER_HOOK_CONFIG" "$patch_path"; then
            fail "$id patch $patch_file must read SEMU_RENDER_HOOK_CONFIG"
          fi
          if ! has_literal "SEMU_RENDER_HOOK_PROOF" "$patch_path"; then
            fail "$id patch $patch_file must support SEMU_RENDER_HOOK_PROOF"
          fi
        if ! has_literal "semu-render-hook:%s:game_framebuffer:config=%s" "$patch_path" &&
           ! has_literal "semu-render-hook:{0}:game_framebuffer:config={1}{2}" "$patch_path"; then
          fail "$id patch $patch_file must emit the generic render hook proof marker"
        fi
        if ! has_literal "semuRenderHookProof(\"$id\")" "$patch_path" &&
           ! has_literal "SemuRenderHookProof(\"$id\")" "$patch_path"; then
          fail "$id patch $patch_file must call the Semu hook at the declared emulator boundary"
        fi
        if [ "$implementation_status" = "implemented_source_hook" ]; then
          if [ "$requires_deck_proof" != "true" ]; then
            fail "$id implemented_source_hook must require Deck proof"
          fi
          if ! has_literal "ComposeFrame" "$patch_path" &&
             ! has_literal "composeFrame" "$patch_path" &&
             ! has_literal "compose_frame" "$patch_path"; then
            fail "$id implemented_source_hook patch $patch_file must compose an output frame"
          fi
          if has_literal "video_shader_enable" "$patch_path" ||
             has_literal "video_shader =" "$patch_path" ||
             has_literal "video_shader_path" "$patch_path"; then
            fail "$id implemented_source_hook patch $patch_file must not use RetroArch-native shader configuration"
          fi
        fi
        done < <(jq -r '.semu_renderer.hook_contract.patch_files[]?' "$rendering")
      fi
    fi
  else
    if [ "$preferred_backend" = "source_hook" ]; then
      fail "$id source_hook_enabled=false must not prefer source_hook"
    fi

    if [ "$generic_renderer" = "true" ]; then
      fail "$id source_hook_enabled=false must not expose generic_renderer capability"
    fi

    case "$package_status" in
      source-package-ready)
        fail "$id source_hook_enabled=false cannot report ready source-hook package status"
        ;;
    esac
  fi
done

if [ "$failed" -ne 0 ]; then
  exit 1
fi

printf '%s\n' "OK source-hook package metadata"
