{ lib, stdenv, makeWrapper, symlinkJoin,
  dolphin-emu, azahar, ares ? null,
  pcsx2 ? null, cemu ? null, ppsspp ? null, flycast ? null,
  melonds ? null, retroarch-bare ? null,
  ryujinx ? null, es-de ? null,
  syncthing ? null, syncthingtray ? null, curl ? null, bubblewrap ? null,
  semuShaderBundle ? null,
  nixGLIntel ? null,
  semuCli ? null,
  routedEmulators ? [],
}:

let
  runtimeTools = lib.filter (x: x != null) [ syncthing syncthingtray curl bubblewrap nixGLIntel ];
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
      if [ -d src/semu/bootstrap/templates ]; then
        mkdir -p $out/lib/semu/src/semu/bootstrap
        cp -r src/semu/bootstrap/templates $out/lib/semu/src/semu/bootstrap/
      fi
      ${stdenv.cc.targetPrefix}cc generated/semu.c -std=c11 -o $out/lib/semu/semu-btrc -lm
      ${if stdenv.hostPlatform.isLinux then ''
        ${stdenv.cc.targetPrefix}cc src/semu/quit-watch.c -std=c11 -O2 -o $out/bin/semu-quit-watch
      '' else ''
        cat > $out/bin/semu-quit-watch <<'WRAPPER'
        #!/usr/bin/env sh
        [ "$1" = "--" ] && shift
        exec "$@"
        WRAPPER
        chmod +x $out/bin/semu-quit-watch
      ''}
      makeWrapper $out/lib/semu/semu-btrc $out/bin/semu \
        --set SEMU_ASSET_ROOT $out/lib/semu \
        --set SEMU_BIN $out/bin/semu \
        --prefix PATH : ${lib.escapeShellArg runtimePath}

      if [ -d packaging/linux ]; then
        mkdir -p $out/lib/semu/packaging
        cp -r packaging/linux $out/lib/semu/packaging/
      fi

      # Standalone-emulator bezel assets (vkBasalt reshade effect + per-aspect bezels). The
      # launcher generates a per-emulator vkBasalt.conf pointing at these and enables vkBasalt.
      if [ -d packaging/standalone-bezel/reshade ]; then
        mkdir -p $out/lib/semu/share/semu-bezel
        cp -r packaging/standalone-bezel/reshade $out/lib/semu/share/semu-bezel/reshade
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
    melonds
    retroarch-bare
    ryujinx
    es-de
    syncthing
    syncthingtray
    curl
    bubblewrap
    nixGLIntel
  ];

  shaderPostBuild = lib.optionalString (semuShaderBundle != null) ''
    if [ -L "$out/share/libretro" ]; then
      existing="$(readlink "$out/share/libretro")"
      rm "$out/share/libretro"
      mkdir -p "$out/share/libretro"
      cp -aL "$existing/." "$out/share/libretro/" 2>/dev/null || true
    else
      mkdir -p "$out/share/libretro"
    fi

    chmod -R u+w "$out/share/libretro" 2>/dev/null || true
    rm -rf "$out/share/libretro/shaders"
    cp -aL ${semuShaderBundle}/share/libretro/shaders "$out/share/libretro/shaders"
  '';
in
symlinkJoin {
  name = "semu-full";
  paths = [ setupTool ] ++ routedEmulators ++ emulators;
  postBuild = shaderPostBuild;
  meta.description = "Semu with all emulators bundled";
}
