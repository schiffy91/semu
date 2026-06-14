{ lib, stdenv, makeWrapper, symlinkJoin,
  dolphin-emu, azahar, ares ? null,
  pcsx2 ? null, cemu ? null, ppsspp ? null, flycast ? null,
  melonds ? null, retroarch-bare ? null,
  ryujinx ? null, es-de ? null,
  syncthing ? null, syncthingtray ? null, curl ? null, bubblewrap ? null,
  gamescope ? null, vkbasalt ? null,
  vulkanLoader ? null,
  semuShaderBundle ? null,
  nixGLDefault ? null,
  semuCli ? null,
  routedEmulators ? [],
}:

let
  runtimeTools = lib.filter (x: x != null) [ syncthing syncthingtray curl bubblewrap gamescope vkbasalt vulkanLoader nixGLDefault ];
  runtimePath = lib.makeBinPath runtimeTools;

  setupTool = if semuCli != null then semuCli else stdenv.mkDerivation {
    pname = "semu";
    version = "0.1.0";
    src = lib.cleanSource ../../..;
    nativeBuildInputs = [ makeWrapper ];
    dontBuild = true;
    installPhase = ''
      mkdir -p $out/bin $out/lib/semu

      cp src/main.btrc $out/lib/semu/main.btrc
      cp semu.json $out/lib/semu/
      mkdir -p $out/lib/semu/build
      cp -r build/generated $out/lib/semu/build/
      for dir in config; do
        if [ -d "$dir" ]; then
          cp -r "$dir" "$out/lib/semu/"
        fi
      done
      if [ -d assets ]; then
        cp -r assets "$out/lib/semu/"
      fi
      ${stdenv.cc.targetPrefix}cc build/generated/semu.c -std=c11 -o $out/lib/semu/semu-btrc -lm
      ${if stdenv.hostPlatform.isLinux then ''
        ${stdenv.cc.targetPrefix}cc build/generated/semu-quit-watch.c -std=c11 -O2 -o $out/bin/semu-quit-watch
        install -Dm755 build/packaging/linux/bin/semu-render $out/bin/semu-render
      '' else ''
        cat > $out/bin/semu-quit-watch <<'WRAPPER'
        #!/usr/bin/env sh
        [ "$1" = "--" ] && shift
        exec "$@"
        WRAPPER
        chmod +x $out/bin/semu-quit-watch
        install -Dm755 build/packaging/linux/bin/semu-render $out/bin/semu-render
      ''}
      makeWrapper $out/lib/semu/semu-btrc $out/bin/semu \
        --set SEMU_ASSET_ROOT $out/lib/semu \
        --set SEMU_BIN $out/bin/semu \
        --prefix PATH : ${lib.escapeShellArg runtimePath}

      cat > $out/bin/semu-settings <<'WRAPPER'
      #!/usr/bin/env sh
      set -eu
      here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
      semu="$here/semu"
      project="''${SEMU_PROJECT_DIR:-$HOME/.local/share/semu}"
      entry="''${1:-}"
      if [ -n "$entry" ] && [ -f "$entry" ]; then
        exec "$semu" settings entry "$entry" --project "$project"
      fi
      exec "$semu" settings ui --project "$project"
      WRAPPER
      chmod +x $out/bin/semu-settings

      if [ -d build/packaging/linux ]; then
        mkdir -p $out/lib/semu/packaging
        cp -r build/packaging/linux $out/lib/semu/packaging/
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
    gamescope
    vkbasalt
    vulkanLoader
    nixGLDefault
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
  '' + lib.optionalString (vkbasalt != null) ''
    mkdir -p "$out/share/vulkan" "$out/lib"
    rm -rf "$out/share/vulkan/implicit_layer.d" "$out/lib/vkbasalt"
    cp -aL ${vkbasalt}/share/vulkan/implicit_layer.d "$out/share/vulkan/implicit_layer.d"
    cp -aL ${vkbasalt}/lib/vkbasalt "$out/lib/vkbasalt"
  '';
in
symlinkJoin {
  name = "semu-full";
  paths = [ setupTool ] ++ routedEmulators ++ emulators;
  postBuild = shaderPostBuild;
  meta.description = "Semu with all emulators bundled";
}
