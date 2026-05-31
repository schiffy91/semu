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
  pname = "schemulator";
  version = "0.1.0";
  src = lib.cleanSource ./..;
  nativeBuildInputs = [ makeWrapper ];
  dontBuild = true;
  installPhase = ''
    mkdir -p $out/bin $out/lib/schemulator

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
    description = "Schemulator BTRC runtime CLI";
    license = lib.licenses.mit;
  };
}
