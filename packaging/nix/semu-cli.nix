{ lib, stdenv, makeWrapper
, syncthing ? null
, syncthingtray ? null
, curl ? null
, bubblewrap ? null
}:

let
  runtimeTools = lib.filter (x: x != null) [ syncthing syncthingtray curl bubblewrap ];
  runtimePath = lib.makeBinPath runtimeTools;
in
stdenv.mkDerivation {
  pname = "semu";
  version = "0.1.0";
  src = lib.cleanSource ../..;
  nativeBuildInputs = [ makeWrapper ];
  dontBuild = true;
  installPhase = ''
    mkdir -p $out/bin $out/lib/semu

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

    # Standalone-emulator bezel assets (vkBasalt reshade effect + per-aspect bezels).
    # Must live under SEMU_ASSET_ROOT ($out/lib/semu); the launcher reads
    # share/semu-bezel/reshade and generates a per-emulator vkBasalt.conf pointing at it.
    if [ -d packaging/standalone-bezel/reshade ]; then
      mkdir -p $out/lib/semu/share/semu-bezel
      cp -r packaging/standalone-bezel/reshade $out/lib/semu/share/semu-bezel/reshade
    fi
  '';
  meta = {
    description = "Semu BTRC runtime CLI";
    license = lib.licenses.mit;
  };
}
