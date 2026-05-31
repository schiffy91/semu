{ lib, stdenv, makeWrapper, symlinkJoin,
  dolphin-emu, azahar, ares ? null,
  pcsx2 ? null, cemu ? null, ppsspp ? null, flycast ? null,
  gopher64 ? null, melonds ? null, retroarch-bare ? null,
  ryujinx ? null, es-de ? null,
  syncthing ? null, syncthingtray ? null, curl ? null, bubblewrap ? null,
  schemulatorCli ? null,
  routedEmulators ? [],
}:

let
  runtimeTools = lib.filter (x: x != null) [ syncthing syncthingtray curl bubblewrap ];
  runtimePath = lib.makeBinPath runtimeTools;

  setupTool = if schemulatorCli != null then schemulatorCli else stdenv.mkDerivation {
    pname = "schemulator";
    version = "0.1.0";
    src = lib.cleanSource ./..;
    nativeBuildInputs = [ makeWrapper ];
    dontBuild = true;
    installPhase = ''
      mkdir -p $out/bin $out/lib/schemulator

      # Copy BTRC runtime sources and generated manifest.
      cp schemulator.btrc schemulator.json $out/lib/schemulator/
      cp -r generated $out/lib/schemulator/
      ${stdenv.cc.targetPrefix}cc generated/schemulator.c -std=c11 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -o $out/lib/schemulator/schemulator-btrc -lm
      makeWrapper $out/lib/schemulator/schemulator-btrc $out/bin/schemulator \
        --set SCHEMULATOR_ASSET_ROOT $out/lib/schemulator \
        --set SCHEMULATOR_BIN $out/bin/schemulator \
        --prefix PATH : ${lib.escapeShellArg runtimePath}

      if [ -d linux ]; then
        cp -r linux $out/lib/schemulator/
      fi
    '';
    meta = {
      description = "Deterministic emulation environment manager";
      license = lib.licenses.mit;
    };
  };

  emulators = lib.filter (x: x != null) [
    dolphin-emu
    azahar
    ares
    pcsx2
    cemu
    ppsspp
    flycast
    gopher64
    melonds
    retroarch-bare
    ryujinx
    es-de
    syncthing
    syncthingtray
    curl
    bubblewrap
  ];
in
symlinkJoin {
  name = "schemulator-full";
  paths = [ setupTool ] ++ routedEmulators ++ emulators;
  meta.description = "Schemulator with all emulators bundled";
}
