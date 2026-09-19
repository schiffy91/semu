# The composed bundle: CLI, ES-DE, emulators, launcher shims. Its root is SEMU_ASSET_ROOT.
{ lib, symlinkJoin, makeWrapper, semuCli, esDe, emulatorPackages ? [ ], extraPackages ? [ ], repositoryRoot }:

let
  systemsDir = repositoryRoot + "/config/systems";
  systemIds = lib.attrNames (lib.filterAttrs (_: type: type == "directory") (builtins.readDir systemsDir));
  systemContracts = map (id: lib.importJSON (systemsDir + "/${id}/system.json")) systemIds;
  linuxEmulators = lib.unique (lib.concatMap (system:
    map (entry: entry.emulator) (lib.filter (entry: entry ? emulator && lib.elem "linux" (entry.platforms or [ "linux" ])) (system.emulators or [ ]))
  ) systemContracts);
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
      --prefix PATH : "$out/bin"
    ${lib.concatMapStrings shim linuxEmulators}

    cat > "$out/bin/semu-es-de" <<LAUNCHER
    #!/bin/sh
    target="\''${SEMU_TARGET:-linux-desktop}"
    "$out/bin/semu" prepare --target "\$target" || exit 1
    "$out/bin/semu" sync start --target "\$target" >/dev/null 2>&1 || true
    home="\$("$out/bin/semu" path esde_home --target "\$target")" || exit 1
    exec "$out/bin/es-de" --home "\$home" "\$@"
    LAUNCHER
    chmod +x "$out/bin/semu-es-de"

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

  passthru = { inherit linuxEmulators; };

  meta = {
    description = "Semu with ES-DE and every selected emulator";
    license = lib.licenses.mit;
    mainProgram = "semu";
  };
}
