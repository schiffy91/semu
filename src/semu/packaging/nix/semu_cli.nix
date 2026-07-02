{ lib, stdenv, makeWrapper, btrcpy
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
  src = lib.cleanSource ../../../..;
  nativeBuildInputs = [ makeWrapper btrcpy ];
  dontBuild = true;
  installPhase = ''
    mkdir -p $out/bin $out/lib/semu

    cp src/semu.btrc $out/lib/semu/semu.btrc
    mkdir -p $out/lib/semu/src
    cp -r src/semu $out/lib/semu/src/
    btrcpy src/semu.btrc -o semu.c --no-cache --no-stdlib
    ${stdenv.cc.targetPrefix}cc semu.c -std=c11 -o $out/lib/semu/semu-btrc -lm
    ${if stdenv.hostPlatform.isLinux then ''
      ${stdenv.cc.targetPrefix}cc src/semu/packaging/appimage/quit_watch.c -std=c11 -O2 -o $out/bin/semu-quit-watch
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

    if [ -d src/semu/emulators/rendering/assets/reshade ]; then
      mkdir -p $out/lib/semu/share/semu-bezel
      cp -r src/semu/emulators/rendering/assets/reshade $out/lib/semu/share/semu-bezel/reshade
    fi
  '';
  meta = {
    description = "Semu BTRC runtime CLI";
    license = lib.licenses.mit;
  };
}
