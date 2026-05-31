#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP="$(mktemp -d -t semu-appimage-smoke.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

BIN_DIR="$TMP/bin"
mkdir -p "$BIN_DIR"

expect_status() {
  local expected="$1"
  shift
  set +e
  "$@" >/dev/null 2>"$TMP/expect.err"
  local status=$?
  set -e
  if [ "$status" -ne "$expected" ]; then
    echo "expected exit $expected, got $status: $*" >&2
    cat "$TMP/expect.err" >&2 || true
    exit 9
  fi
}

cat > "$TMP/fake-esde.AppImage" <<'SH'
#!/usr/bin/env bash
set -euo pipefail

if [ "${1:-}" != "--appimage-extract" ]; then
  echo "fake ES-DE AppImage only supports --appimage-extract" >&2
  exit 2
fi

mkdir -p squashfs-root/usr/bin squashfs-root/usr/share/applications squashfs-root/usr/lib
cat > squashfs-root/usr/bin/es-de <<'EOF'
#!/usr/bin/env bash
echo fake es-de "$@"
EOF
chmod +x squashfs-root/usr/bin/es-de
printf 'fake icon\n' > squashfs-root/semu.png
SH
chmod +x "$TMP/fake-esde.AppImage"

cat > "$BIN_DIR/appimagetool" <<'SH'
#!/usr/bin/env bash
set -euo pipefail

if [ "${1:-}" = "--no-appstream" ]; then
  shift
fi
APPDIR="${1:?missing AppDir}"
OUTPUT="${2:?missing output}"

for required in \
    AppRun \
    usr/bin/es-de \
    usr/bin/semu \
    usr/bin/bwrap \
    usr/bin/semu-retroarch \
    usr/bin/semu-dolphin \
    usr/bin/semu-ppsspp \
    usr/bin/semu-flycast \
    usr/bin/semu-gopher64 \
    usr/bin/semu-melonds \
    usr/bin/semu-pcsx2 \
    usr/bin/semu-cemu \
    usr/bin/semu-azahar \
    usr/bin/semu-ryujinx \
    usr/bin/semu-btrc \
    usr/bin/semu-es-de \
    nix/store \
    linux/AppRun \
    linux/ES-DE/es_find_rules_linux.xml; do
  test -e "$APPDIR/$required" || {
    echo "missing AppDir path: $required" >&2
    exit 3
  }
done

test ! -e "$APPDIR/linux/build-appimage.sh" || {
  echo "build script must not be shipped inside AppDir" >&2
  exit 3
}

grep -F 'SEMU_NIX_STORE_MOUNTED' "$APPDIR/AppRun" >/dev/null
grep -F -- '--ro-bind "$APPDIR/nix/store" /nix/store' "$APPDIR/AppRun" >/dev/null
grep -F 'SEMU_LAUNCHER_BIN' "$APPDIR/AppRun" >/dev/null
! grep -F 'sync setup' "$APPDIR/AppRun" >/dev/null
! grep -F 'pkexec' "$APPDIR/AppRun" >/dev/null

find "$APPDIR/usr/bin" -maxdepth 1 -type f -perm -111 -print | sort > "$OUTPUT"

while IFS= read -r launcher; do
  test -x "$APPDIR/usr/bin/$launcher" || test -x "$APPDIR/linux/bin/$launcher" || {
    echo "generated ES-DE launcher has no executable: $launcher" >&2
    exit 3
  }
done < <(grep -o 'semu-[a-z0-9-]*' "$APPDIR/linux/ES-DE/es_find_rules_linux.xml" | sort -u)
SH
chmod +x "$BIN_DIR/appimagetool"

cat > "$BIN_DIR/nix" <<'SH'
#!/usr/bin/env bash
set -euo pipefail

if [ "${1:-}" != "copy" ]; then
  echo "fake nix only supports copy" >&2
  exit 2
fi

ROOT=""
while [ $# -gt 0 ]; do
  case "$1" in
    --to)
      ROOT="${2#local?root=}"
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done

test -n "$ROOT" || {
  echo "fake nix copy missing local root" >&2
  exit 2
}
mkdir -p "$ROOT/nix/store/fake-semu-closure"
printf 'fake closure\n' > "$ROOT/nix/store/fake-semu-closure/marker"
SH
chmod +x "$BIN_DIR/nix"

NIX_PACKAGE="$TMP/nix-package"
mkdir -p "$NIX_PACKAGE/bin"
for bin in \
    semu \
    bwrap \
    semu-retroarch \
    semu-dolphin \
    semu-ppsspp \
    semu-flycast \
    semu-gopher64 \
    semu-melonds \
    semu-pcsx2 \
    semu-cemu \
    semu-azahar \
    semu-ryujinx \
    semu-es-de; do
  cat > "$NIX_PACKAGE/bin/$bin" <<SH
#!/usr/bin/env bash
echo fake $bin "\$@"
SH
  chmod +x "$NIX_PACKAGE/bin/$bin"
done

PARTIAL_NIX_PACKAGE="$TMP/partial-nix-package"
mkdir -p "$PARTIAL_NIX_PACKAGE/bin"
for bin in \
    semu \
    bwrap \
    semu-retroarch \
    semu-dolphin \
    semu-flycast \
    semu-gopher64 \
    semu-melonds \
    semu-pcsx2 \
    semu-cemu \
    semu-azahar \
    semu-ryujinx \
    semu-es-de; do
  cat > "$PARTIAL_NIX_PACKAGE/bin/$bin" <<SH
#!/usr/bin/env bash
echo fake $bin "\$@"
SH
  chmod +x "$PARTIAL_NIX_PACKAGE/bin/$bin"
done

NO_BWRAP_NIX_PACKAGE="$TMP/no-bwrap-nix-package"
mkdir -p "$NO_BWRAP_NIX_PACKAGE/bin"
for bin in semu semu-es-de; do
  cat > "$NO_BWRAP_NIX_PACKAGE/bin/$bin" <<SH
#!/usr/bin/env bash
echo fake $bin "\$@"
SH
  chmod +x "$NO_BWRAP_NIX_PACKAGE/bin/$bin"
done
expect_status 4 env \
  PATH="$BIN_DIR:$PATH" \
  APPIMAGETOOL="$BIN_DIR/appimagetool" \
  "$REPO_ROOT/linux/build-appimage.sh" \
  --nix-package "$NO_BWRAP_NIX_PACKAGE" \
  --esde-appimage "$TMP/fake-esde.AppImage" \
  --output "$TMP/no-bwrap-nix.AppImage" \
  --arch x86_64

PARTIAL_OUTPUT="$TMP/Semu-partial.AppImage"
PATH="$BIN_DIR:$PATH" \
APPIMAGETOOL="$BIN_DIR/appimagetool" \
"$REPO_ROOT/linux/build-appimage.sh" \
  --nix-package "$PARTIAL_NIX_PACKAGE" \
  --esde-appimage "$TMP/fake-esde.AppImage" \
  --output "$PARTIAL_OUTPUT" \
  --arch x86_64
grep -F /usr/bin/semu-ppsspp "$PARTIAL_OUTPUT" >/dev/null

OUTPUT="$TMP/Semu-test.AppImage"
PATH="$BIN_DIR:$PATH" \
APPIMAGETOOL="$BIN_DIR/appimagetool" \
"$REPO_ROOT/linux/build-appimage.sh" \
  --nix-package "$NIX_PACKAGE" \
  --esde-appimage "$TMP/fake-esde.AppImage" \
  --output "$OUTPUT" \
  --arch x86_64

for expected in \
    /usr/bin/es-de \
    /usr/bin/semu \
    /usr/bin/bwrap \
    /usr/bin/semu-retroarch \
    /usr/bin/semu-dolphin \
    /usr/bin/semu-ppsspp \
    /usr/bin/semu-flycast \
    /usr/bin/semu-gopher64 \
    /usr/bin/semu-melonds \
    /usr/bin/semu-pcsx2 \
    /usr/bin/semu-cemu \
    /usr/bin/semu-azahar \
    /usr/bin/semu-ryujinx \
    /usr/bin/semu-btrc \
    /usr/bin/semu-es-de; do
  grep -F "$expected" "$OUTPUT" >/dev/null || {
    echo "missing executable in AppImage smoke output: $expected" >&2
    exit 4
  }
done

APPDIR="$TMP/AppRunProbe.AppDir"
mkdir -p "$APPDIR/nix/store" "$APPDIR/usr/bin"
cp "$REPO_ROOT/linux/AppRun" "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"

cat > "$BIN_DIR/bwrap" <<SH
#!/usr/bin/env bash
printf '%s\n' "\$@" > "$TMP/bwrap.args"
SH
chmod +x "$BIN_DIR/bwrap"

APPDIR="$APPDIR" \
SEMU_BWRAP="$BIN_DIR/bwrap" \
"$APPDIR/AppRun" --probe

grep -F -- '--tmpfs' "$TMP/bwrap.args" >/dev/null
grep -F -- '--ro-bind' "$TMP/bwrap.args" >/dev/null
grep -F "$APPDIR/nix/store" "$TMP/bwrap.args" >/dev/null
grep -F '/nix/store' "$TMP/bwrap.args" >/dev/null
grep -F "$APPDIR/AppRun" "$TMP/bwrap.args" >/dev/null

NO_PROJECT_APPDIR="$TMP/AppRunNoProject.AppDir"
mkdir -p "$NO_PROJECT_APPDIR/usr/bin" "$TMP/no-project-home"
cp "$REPO_ROOT/linux/AppRun" "$NO_PROJECT_APPDIR/AppRun"
chmod +x "$NO_PROJECT_APPDIR/AppRun"
expect_status 1 env \
  APPDIR="$NO_PROJECT_APPDIR" \
  HOME="$TMP/no-project-home" \
  "$NO_PROJECT_APPDIR/AppRun"

MISSING_CLI_APPDIR="$TMP/AppRunMissingCli.AppDir"
MISSING_CLI_PROJECT="$TMP/missing-cli-project"
mkdir -p "$MISSING_CLI_APPDIR/usr/bin" "$MISSING_CLI_PROJECT"
printf '{"schema_version":1}\n' > "$MISSING_CLI_PROJECT/semu.json"
cp "$REPO_ROOT/linux/AppRun" "$MISSING_CLI_APPDIR/AppRun"
chmod +x "$MISSING_CLI_APPDIR/AppRun"
expect_status 2 env \
  APPDIR="$MISSING_CLI_APPDIR" \
  SEMU_PROJECT_DIR="$MISSING_CLI_PROJECT" \
  "$MISSING_CLI_APPDIR/AppRun"

BWRAP_MISSING_APPDIR="$TMP/AppRunMissingBwrap.AppDir"
mkdir -p "$BWRAP_MISSING_APPDIR/nix/store"
cp "$REPO_ROOT/linux/AppRun" "$BWRAP_MISSING_APPDIR/AppRun"
chmod +x "$BWRAP_MISSING_APPDIR/AppRun"
expect_status 127 env \
  PATH="/usr/bin:/bin" \
  APPDIR="$BWRAP_MISSING_APPDIR" \
  "$BWRAP_MISSING_APPDIR/AppRun"

expect_status 4 env \
  PATH="$BIN_DIR:$PATH" \
  APPIMAGETOOL="$BIN_DIR/appimagetool" \
  "$REPO_ROOT/linux/build-appimage.sh" \
  --nix-package "$TMP/missing-nix-package" \
  --esde-appimage "$TMP/fake-esde.AppImage" \
  --output "$TMP/missing-nix.AppImage" \
  --arch x86_64

NO_CLI_ROOT="$TMP/no-cli-root"
mkdir -p "$NO_CLI_ROOT"
cp -R "$REPO_ROOT/linux" "$NO_CLI_ROOT/linux"
expect_status 5 env \
  PATH="$BIN_DIR:$PATH" \
  APPIMAGETOOL="$BIN_DIR/appimagetool" \
  "$NO_CLI_ROOT/linux/build-appimage.sh" \
  --esde-appimage "$TMP/fake-esde.AppImage" \
  --output "$TMP/no-cli.AppImage" \
  --arch x86_64

CLI_APPDIR="$TMP/AppRunCli.AppDir"
CLI_PROJECT="$TMP/cli-project"
mkdir -p "$CLI_APPDIR/usr/bin" "$CLI_APPDIR/linux/bin" "$CLI_PROJECT"
printf '{"schema_version":1}\n' > "$CLI_PROJECT/semu.json"
cp "$REPO_ROOT/linux/AppRun" "$CLI_APPDIR/AppRun"
chmod +x "$CLI_APPDIR/AppRun"
cat > "$CLI_APPDIR/usr/bin/semu" <<SH
#!/usr/bin/env bash
printf '%s\n' "\$@" > "$TMP/cli.args"
SH
chmod +x "$CLI_APPDIR/usr/bin/semu"

APPDIR="$CLI_APPDIR" \
SEMU_PROJECT_DIR="$CLI_PROJECT" \
"$CLI_APPDIR/AppRun" manifest --output "$TMP/manifest.json"
grep -F 'manifest' "$TMP/cli.args" >/dev/null
grep -F -- '--output' "$TMP/cli.args" >/dev/null
grep -F "$TMP/manifest.json" "$TMP/cli.args" >/dev/null
grep -F -- '--project' "$TMP/cli.args" >/dev/null
grep -F "$CLI_PROJECT" "$TMP/cli.args" >/dev/null

RUN_APPDIR="$TMP/AppRunLaunch.AppDir"
RUN_PROJECT="$TMP/run-project"
RUN_HOME="$TMP/run-home"
mkdir -p "$RUN_APPDIR/usr/bin" "$RUN_APPDIR/linux/bin" "$RUN_PROJECT/ES-DE/custom_systems" "$RUN_HOME"
printf '{"schema_version":1}\n' > "$RUN_PROJECT/semu.json"
printf '<systemList />\n' > "$RUN_PROJECT/ES-DE/custom_systems/es_systems.xml"
printf '<ruleList />\n' > "$RUN_PROJECT/ES-DE/custom_systems/es_find_rules.xml"
cp "$REPO_ROOT/linux/AppRun" "$RUN_APPDIR/AppRun"
chmod +x "$RUN_APPDIR/AppRun"
cat > "$RUN_APPDIR/usr/bin/semu" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
if [ "${1:-}" = "config" ] && [ "${2:-}" = "env" ]; then
  printf 'export SEMU_PROJECT_DIR=%q\n' "${SEMU_PROJECT_DIR:?}"
  printf 'export SEMU_ROMS_DIR=%q\n' "$SEMU_PROJECT_DIR/roms"
  printf 'export SEMU_BIN=%q\n' "$0"
  exit 0
fi
if [ "${1:-}" = "apprun" ] && [ "${2:-}" = "prepare" ]; then
  mkdir -p "$HOME/ES-DE/custom_systems" "$HOME/ES-DE/settings"
  printf '<systemList />\n' > "$HOME/ES-DE/custom_systems/es_systems.xml"
  printf '<ruleList><entry>%s/semu-retroarch</entry></ruleList>\n' "${SEMU_LAUNCHER_BIN:?}" \
    > "$HOME/ES-DE/custom_systems/es_find_rules.xml"
  printf '<string name="ROMDirectory" value="%s" />\n' "${SEMU_ROMS_DIR:?}" \
    > "$HOME/ES-DE/settings/es_settings.xml"
  exit 0
fi
printf 'unexpected cli: %s\n' "$*" > "${SEMU_APP_RUN_UNEXPECTED:?}"
exit 9
SH
chmod +x "$RUN_APPDIR/usr/bin/semu"
cat > "$RUN_APPDIR/usr/bin/es-de" <<SH
#!/usr/bin/env bash
printf '%s\n' "\$@" > "$TMP/esde.args"
SH
chmod +x "$RUN_APPDIR/usr/bin/es-de"
cat > "$RUN_APPDIR/usr/bin/semu-retroarch" <<'SH'
#!/usr/bin/env bash
exit 0
SH
chmod +x "$RUN_APPDIR/usr/bin/semu-retroarch"

APPDIR="$RUN_APPDIR" \
HOME="$RUN_HOME" \
SEMU_PROJECT_DIR="$RUN_PROJECT" \
SEMU_APP_RUN_UNEXPECTED="$TMP/app-run-unexpected" \
"$RUN_APPDIR/AppRun"
test ! -e "$TMP/app-run-unexpected"
cmp -s "$RUN_PROJECT/ES-DE/custom_systems/es_systems.xml" "$RUN_HOME/ES-DE/custom_systems/es_systems.xml"
grep -F "$RUN_APPDIR/usr/bin/semu-retroarch" "$RUN_HOME/ES-DE/custom_systems/es_find_rules.xml" >/dev/null
grep -F "$RUN_PROJECT/roms" "$RUN_HOME/ES-DE/settings/es_settings.xml" >/dev/null
grep -F -- '--resolution' "$TMP/esde.args" >/dev/null

BOOTSTRAP_PROJECT="$TMP/bootstrap-project"
mkdir -p "$BOOTSTRAP_PROJECT"
SEMU_LAUNCHER_BIN="$APPDIR/usr/bin" \
"$REPO_ROOT/build/semu" bootstrap --project "$BOOTSTRAP_PROJECT" >/dev/null
grep -F "$APPDIR/usr/bin/semu-retroarch" \
  "$BOOTSTRAP_PROJECT/ES-DE/custom_systems/es_find_rules.xml" >/dev/null

echo "OK AppImage assembly smoke"
