{ lib, stdenv, makeWrapper, symlinkJoin,
  dolphin-emu, azahar, ares ? null,
  pcsx2 ? null, cemu ? null, ppsspp ? null, flycast ? null,
  gopher64 ? null, melonds ? null, retroarch-bare ? null,
  ryujinx ? null, es-de ? null,
  syncthing ? null, syncthingtray ? null, curl ? null, bubblewrap ? null,
  semuCli ? null,
  routedEmulators ? [],
}:

let
  runtimeTools = lib.filter (x: x != null) [ syncthing syncthingtray curl bubblewrap ];
  runtimePath = lib.makeBinPath runtimeTools;

  setupTool = if semuCli != null then semuCli else stdenv.mkDerivation {
    pname = "semu";
    version = "0.1.0";
    src = lib.cleanSource ../..;
    nativeBuildInputs = [ makeWrapper ];
    dontBuild = true;
    installPhase = ''
      mkdir -p $out/bin $out/lib/semu

      # Copy BTRC runtime sources and generated manifest.
      cp src/semu.btrc $out/lib/semu/semu.btrc
      cp semu.json $out/lib/semu/
      cp -r generated $out/lib/semu/
      ${stdenv.cc.targetPrefix}cc generated/semu.c -std=c11 -o $out/lib/semu/semu-btrc -lm
      makeWrapper $out/lib/semu/semu-btrc $out/bin/semu \
        --set SEMU_ASSET_ROOT $out/lib/semu \
        --set SEMU_BIN $out/bin/semu \
        --prefix PATH : ${lib.escapeShellArg runtimePath}

      if [ -d packaging/linux ]; then
        mkdir -p $out/lib/semu/packaging
        cp -r packaging/linux $out/lib/semu/packaging/
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
  name = "semu-full";
  paths = [ setupTool ] ++ routedEmulators ++ emulators;
  meta.description = "Semu with all emulators bundled";
}
