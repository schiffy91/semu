# The composed bundle: CLI, ES-DE, emulators, launcher shims. Its root is SEMU_ASSET_ROOT.
# `platform` picks the system bindings and the default target: linux-desktop or macos.
{ lib, symlinkJoin, makeWrapper, semuCli, esDe, emulatorPackages ? [ ], extraPackages ? [ ], repositoryRoot, platform ? "linux" }:

let
  systemsDir = repositoryRoot + "/config/systems";
  systemIds = lib.attrNames (lib.filterAttrs (_: type: type == "directory") (builtins.readDir systemsDir));
  systemContracts = map (id: lib.importJSON (systemsDir + "/${id}/system.json")) systemIds;
  platformEmulators = lib.unique (lib.concatMap (system:
    map (entry: entry.emulator) (lib.filter (entry: entry ? emulator && lib.elem platform (entry.platforms or [ platform ])) (system.emulators or [ ]))
  ) systemContracts);
  defaultTarget = if platform == "macos" then "macos" else "linux-desktop";
  shim = emulator: ''
    cat > "$out/bin/semu-${emulator}" <<SHIM
    #!/bin/sh
    exec "$out/bin/semu" launch ${emulator} "\$@"
    SHIM
    chmod +x "$out/bin/semu-${emulator}"
  '';
in
symlinkJoin {
  name = "semu";
  paths = [ semuCli esDe ] ++ emulatorPackages ++ extraPackages;
  nativeBuildInputs = [ makeWrapper ];

  postBuild = ''
    rm -f "$out/bin/semu"
    makeWrapper "$out/lib/semu/semu-btrc" "$out/bin/semu" \
      --set SEMU_ASSET_ROOT "$out" \
      --set SEMU_SOURCE_ROOT "$out/share/semu/config" \
      --set-default SEMU_TARGET "${defaultTarget}" \
      --prefix PATH : "$out/bin"
    ${lib.concatMapStrings shim platformEmulators}

    cat > "$out/bin/semu-es-de" <<LAUNCHER
    #!/bin/sh
    export PATH="$out/bin:\$PATH"  # ES-DE finds the semu-* shims on PATH; an app opened from Finder has only /usr/bin:/bin
    target="\''${SEMU_TARGET:-${defaultTarget}}"
    export SEMU_TARGET="\$target"  # games ES-DE starts run through the semu-* shims, which must use the same target
    "$out/bin/semu" prepare --target "\$target" || exit 1
    "$out/bin/semu" sync start --target "\$target" >/dev/null 2>&1 || true
    home="\$("$out/bin/semu" path esde_home --target "\$target")" || exit 1
    ${lib.optionalString (platform == "macos") ''/usr/bin/defaults write org.es-de.frontend ApplePersistenceIgnoreState -bool YES  # after a crash macOS would hold ES-DE at a modal "reopen windows?" prompt''}
    exec "$out/bin/es-de" --home "\$home" "\$@"
    LAUNCHER
    chmod +x "$out/bin/semu-es-de"

  '' + lib.optionalString (platform == "macos") ''
    mkdir -p "$out/Applications/Semu.app/Contents/MacOS"
    cat > "$out/Applications/Semu.app/Contents/MacOS/Semu" <<APP
    #!/bin/sh
    exec "$out/bin/semu-es-de" "\$@"
    APP
    chmod +x "$out/Applications/Semu.app/Contents/MacOS/Semu"
    cat > "$out/Applications/Semu.app/Contents/Info.plist" <<PLIST
    <?xml version="1.0" encoding="UTF-8"?>
    <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    <plist version="1.0"><dict>
    <key>CFBundleExecutable</key><string>Semu</string>
    <key>CFBundleIdentifier</key><string>org.semu.frontend</string>
    <key>CFBundleName</key><string>Semu</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>LSMinimumSystemVersion</key><string>11.0</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
    <key>NSBluetoothAlwaysUsageDescription</key><string>ES-DE shows the Bluetooth status and emulators read Bluetooth controllers.</string>
    <key>NSMicrophoneUsageDescription</key><string>Emulators pass the microphone to games that use one.</string>
    <key>NSCameraUsageDescription</key><string>Emulators pass the camera to games that use one.</string>
    </dict></plist>
    PLIST
  '' + lib.optionalString (platform == "linux") ''
    mkdir -p "$out/share/applications"
    cat > "$out/share/applications/semu.desktop" <<DESKTOP
    [Desktop Entry]
    Type=Application
    Name=Semu
    Comment=Emulation frontend with every emulator bundled
    Exec=$out/bin/semu-es-de
    Icon=es-de
    Categories=Game;Emulator;
    DESKTOP
  '';

  passthru = { inherit platformEmulators; };

  meta = {
    description = "Semu with ES-DE and every selected emulator";
    license = lib.licenses.mit;
    mainProgram = "semu";
  };
}
