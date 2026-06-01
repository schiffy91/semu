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
    description = "Semu BTRC runtime CLI";
    license = lib.licenses.mit;
  };
}
