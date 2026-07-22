{
  lib,
  stdenvNoCC,
  coreutils,
  diffutils,
  findutils,
  repositoryRoot,
  semuProgram,
  selectedIds,
  packageAttribute,
}:

let
  aggregate = packageAttribute == "steamdeck-runtime";
  selectedEmulator = if aggregate then null else builtins.head selectedIds;
  emulatorStatement = lib.optionalString (selectedEmulator != null) ''
    packageArguments+=(--emulator ${lib.escapeShellArg selectedEmulator})
  '';
  launcherNames = map (id: "semu-${id}") selectedIds;
  expectedRootEntries = [
    "AppRun"
    "bin"
    "semu-launcher"
    "semu.desktop"
    "settings"
  ];
  expectedSettingsEntries = [
    "legacy_shortcuts.json"
    "semu_settings.json"
  ];
  expectedEntries = entries: lib.concatMapStringsSep " " lib.escapeShellArg entries;
  source = lib.fileset.toSource {
    root = repositoryRoot;
    fileset = lib.fileset.unions [
      (repositoryRoot + "/config")
      (repositoryRoot + "/packaging/appimage/AppRun.template")
      (repositoryRoot + "/packaging/appimage/semu-launcher.template")
      (repositoryRoot + "/packaging/appimage/semu.desktop.template")
      (repositoryRoot + "/packaging/install/legacy_shortcuts.json")
      (repositoryRoot + "/tests/targets/steamdeck")
    ];
  };
in
assert lib.assertMsg (
  selectedIds != [ ]
) "AppImage packaging requires at least one selected emulator";
assert lib.assertMsg (
  lib.unique selectedIds == selectedIds
) "AppImage packaging requires unique selected emulator IDs";
assert lib.assertMsg (
  aggregate || builtins.length selectedIds == 1
) "an AppImage emulator slice must select exactly one emulator";
assert lib.assertMsg (
  aggregate || packageAttribute == "steamdeck-runtime-${selectedEmulator}"
) "the AppImage package attribute must identify its selected emulator";
stdenvNoCC.mkDerivation {
  pname = "semu-appimage-packaging-${packageAttribute}";
  version = "0.1.0";
  src = source;

  nativeBuildInputs = [
    coreutils
    diffutils
    findutils
  ];

  dontConfigure = true;
  dontBuild = true;
  dontFixup = true;

  installPhase = ''
    runHook preInstall

    export HOME="$TMPDIR/home"
    export SEMU_HOME="$TMPDIR/semu-home"
    unset SEMU_PROJECT_DIR SEMU_SOURCE_ROOT
    mkdir -p "$HOME" "$SEMU_HOME"

    packageArguments=(
      build package
      --target steam-deck
      --project "$PWD"
    )
    ${emulatorStatement}
    ${semuProgram}/lib/semu/semu-btrc "''${packageArguments[@]}"

    generated="$PWD/build/packaging/linux"
    destination="$out/share/semu/appimage-packaging"

    test -d "$generated"
    test -z "$(find "$generated" -type l -print -quit)"

    find "$generated" -mindepth 1 -maxdepth 1 -printf '%f\n' \
      | LC_ALL=C sort > actual-root-entries
    printf '%s\n' ${expectedEntries expectedRootEntries} \
      | LC_ALL=C sort > expected-root-entries
    cmp actual-root-entries expected-root-entries

    find "$generated/settings" -mindepth 1 -maxdepth 1 -printf '%f\n' \
      | LC_ALL=C sort > actual-settings-entries
    printf '%s\n' ${expectedEntries expectedSettingsEntries} \
      | LC_ALL=C sort > expected-settings-entries
    cmp actual-settings-entries expected-settings-entries

    find "$generated/bin" -mindepth 1 -maxdepth 1 -printf '%f\n' \
      | LC_ALL=C sort > actual-launchers
    printf '%s\n' ${expectedEntries launcherNames} \
      | LC_ALL=C sort > expected-launchers
    cmp actual-launchers expected-launchers

    for executable in \
      "$generated/AppRun" \
      "$generated/semu-launcher" \
      "$generated"/bin/*; do
      test -f "$executable"
      test ! -L "$executable"
      test -x "$executable"
      test "$(stat -c '%a' "$executable")" = 755
    done

    for data in \
      "$generated/semu.desktop" \
      "$generated/settings/legacy_shortcuts.json" \
      "$generated/settings/semu_settings.json"; do
      test -f "$data"
      test ! -L "$data"
      test ! -x "$data"
      test "$(stat -c '%a' "$data")" = 644
    done

    mkdir -p "$destination"
    cp --archive --no-preserve=ownership "$generated/." "$destination/"

    test -z "$(find "$out" -type l -print -quit)"
    diff --recursive --no-dereference "$generated" "$destination"

    runHook postInstall
  '';

  passthru = {
    inherit packageAttribute selectedIds;
  };

  meta = {
    description = "Compiler-generated Semu AppImage packaging for ${packageAttribute}";
    license = lib.licenses.mit;
  };
}
