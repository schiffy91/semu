#!/usr/bin/env bash
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
SEMU_BIN="${SEMU_BIN:-$ROOT/build/out/semu}"

if [ ! -x "$SEMU_BIN" ]; then
  printf '%s\n' "SEMU_BIN is not executable: $SEMU_BIN" >&2
  exit 127
fi

tmp="$(mktemp -d "${TMPDIR:-/tmp}/semu-esde-settings.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT

project="$tmp/project"
home="$tmp/home"
mkdir -p "$project" "$home"
project="$(CDPATH= cd -- "$project" && pwd -P)"
home="$(CDPATH= cd -- "$home" && pwd -P)"

env HOME="$home" SEMU_ASSET_ROOT="$ROOT" SEMU_PROJECT_DIR="$project" \
  "$SEMU_BIN" build configs --project "$project" > "$tmp/build.log"

systems="$project/ES-DE/custom_systems/es_systems.xml"
find_rules="$project/ES-DE/custom_systems/es_find_rules.xml"
settings_script="$project/.semu/generated/bin/semu-settings"
settings_config="$project/.semu/semu.json"
source_settings_config="$ROOT/config/settings/semu-settings.json"

if [ ! -f "$settings_config" ]; then
  printf '%s\n' "Semu Settings config was not generated: $settings_config" >&2
  exit 1
fi
if ! grep -F '"ui_settings_entry": "es-de"' "$source_settings_config" >/dev/null; then
  printf '%s\n' "source Semu Settings config must keep ui_settings_entry=es-de" >&2
  exit 1
fi
if [ "$("$SEMU_BIN" settings get ui_settings_entry --project "$project")" != "es-de" ]; then
  printf '%s\n' "generated Semu Settings config is not backed by config/settings/semu-settings.json" >&2
  exit 1
fi

if grep -F '<name>semu-settings</name>' "$systems" >/dev/null; then
  printf '%s\n' "Semu Settings must not be generated as an ES-DE pseudo-system" >&2
  exit 1
fi
if grep -F '<fullname>Semu Settings</fullname>' "$systems" >/dev/null; then
  printf '%s\n' "Semu Settings leaked into ES-DE systems XML" >&2
  exit 1
fi
if grep -F '<emulator name="SEMU_SETTINGS">' "$find_rules" >/dev/null; then
  printf '%s\n' "Semu Settings must not be generated as an ES-DE emulator find rule" >&2
  exit 1
fi
if grep -F '%EMULATOR_SEMU_SETTINGS%' "$systems" "$find_rules" >/dev/null; then
  printf '%s\n' "Semu Settings must not use ES-DE ROM command routing" >&2
  exit 1
fi

# Historical ROM-command settings entries. Semu Settings should now be one
# ES-DE Utilities menu action, not a generated settings pseudo-system.
obsolete_entries=(
  "Semu Settings.semu"
  "3DS Crypto Status.semu"
  "Apply Settings.semu"
  "Compile Settings.semu"
  "Open Input Settings.semu"
  "Open Presentation Settings.semu"
  "Open Semu Settings.semu"
  "Open Sync Settings.semu"
  "Open Syncthing UI.semu"
  "Open Visual Settings.semu"
  "Sync Status.semu"
  "Toggle Bezels.semu"
  "Toggle CRT Shaders.semu"
  "Toggle Integer Scaling.semu"
  "Toggle ROM Sync.semu"
  "Toggle Syncthing.semu"
  "Use Steam Deck SD ROMs.semu"
)
for entry in "${obsolete_entries[@]}"; do
  if grep -F "$entry" "$systems" "$find_rules" "$settings_script" >/dev/null; then
    printf '%s\n' "obsolete ES-DE settings entry name leaked into generated files: $entry" >&2
    exit 1
  fi
done

if grep -Ei 'fusemount|fusermount|fuse|APPIMAGE_EXTRACT_AND_RUN' "$settings_script" >/dev/null; then
  printf '%s\n' "generated settings launcher must not use FUSE paths" >&2
  exit 1
fi
if grep -F '${SEMU_BIN:-}' "$settings_script" >/dev/null; then
  printf '%s\n' "generated settings launcher must not fall back through SEMU_BIN/AppRun" >&2
  exit 1
fi
if grep -Ei 'fusemount|fusermount|APPIMAGE_EXTRACT_AND_RUN' "$ROOT/build/packaging/linux/AppRun" >/dev/null; then
  printf '%s\n' "AppRun must not require FUSE/fusermount compatibility hooks for settings" >&2
  exit 1
fi
if grep -REi 'fusemount|fusermount|fuse|APPIMAGE_EXTRACT_AND_RUN' "$project/.semu/generated/bin" >/dev/null; then
  printf '%s\n' "generated stable launchers must not use FUSE paths" >&2
  exit 1
fi

patch_file="$ROOT/build/packaging/nix/patches/es-de/001-semu-settings-utility.patch"
grep -F 'SEMU SETTINGS' "$patch_file" >/dev/null
grep -F 'SEMU_ESDE_SETTINGS_COMMAND' "$patch_file" >/dev/null
grep -F 'SEMU_PROJECT_DIR' "$patch_file" >/dev/null
grep -F 'semuShellQuote' "$patch_file" >/dev/null
grep -F 'runSystemCommand(semuShellQuote(semuSettingsCommand) + " &")' "$patch_file" >/dev/null

runner="$project/.semu/generated/bin/semu-btrc"
cat > "$runner" <<'SH'
#!/usr/bin/env sh
: "${SEMU_TEST_ARGS:?}"
printf '%s\n' "$@" > "$SEMU_TEST_ARGS"
SH
chmod +x "$runner"

appimage_runner="$tmp/fail-if-appimage"
cat > "$appimage_runner" <<'SH'
#!/usr/bin/env sh
printf '%s\n' "settings launcher used AppImage fallback unexpectedly" >&2
exit 99
SH
chmod +x "$appimage_runner"
cp "$appimage_runner" "$appimage_runner.AppImage"
chmod +x "$appimage_runner.AppImage"

if grep -F 'SEMU_RUNTIME_CLI' "$settings_script" "$project/.semu/generated/bin"/semu-* >/dev/null; then
  printf '%s\n' "generated launchers must not use legacy SEMU_RUNTIME_CLI" >&2
  exit 1
fi

packaged_runner="$home/semu/result-full/bin/semu"
mkdir -p "$(dirname "$packaged_runner")"
cat > "$packaged_runner" <<'SH'
#!/usr/bin/env sh
: "${SEMU_TEST_ARGS:?}"
printf '%s\n' "$@" > "$SEMU_TEST_ARGS"
SH
chmod +x "$packaged_runner"

expected="$tmp/expected-args"
rm -f "$tmp/args"
HOME="$home" SEMU_CLI="$appimage_runner.AppImage" SEMU_TEST_ARGS="$tmp/args" \
  "$ROOT/build/packaging/linux/bin/semu-btrc" settings ui
{
  printf '%s\n' "settings"
  printf '%s\n' "ui"
} > "$expected"
if ! cmp -s "$expected" "$tmp/args"; then
  diff -u "$expected" "$tmp/args" >&2
  exit 1
fi

SEMU_PROJECT_DIR="$project" SEMU_CLI="$runner" SEMU_BIN="$appimage_runner" SEMU_TEST_ARGS="$tmp/args" SEMU_SETTINGS_NO_TERMINAL=1 "$settings_script"

rm -f "$tmp/args"
SEMU_PROJECT_DIR="$project" SEMU_CLI="$appimage_runner.AppImage" SEMU_TEST_ARGS="$tmp/args" SEMU_SETTINGS_NO_TERMINAL=1 "$settings_script" >/dev/null 2>"$tmp/appimage-stderr"
if grep -F "settings launcher used AppImage fallback unexpectedly" "$tmp/appimage-stderr" >/dev/null; then
  printf '%s\n' "generated settings launcher ran an AppImage-valued SEMU_CLI" >&2
  exit 1
fi

SEMU_PROJECT_DIR="$project" SEMU_CLI="$runner" SEMU_TEST_ARGS="$tmp/args" SEMU_SETTINGS_NO_TERMINAL=1 "$settings_script"

{
  printf '%s\n' "settings"
  printf '%s\n' "ui"
  printf '%s\n' "--project"
  printf '%s\n' "$project"
} > "$expected"

if ! cmp -s "$expected" "$tmp/args"; then
  diff -u "$expected" "$tmp/args" >&2
  exit 1
fi

printf '%s\n' "OK ES-DE Semu Settings entry"
