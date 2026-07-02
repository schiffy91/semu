# semu_cli.nix — the BTRC runtime CLI: transpile src/semu.btrc with btrcpy,
# compile it, and wrap it with the runtime tool PATH. SEMU_ASSET_ROOT points
# at the package's own lib/semu; the composed bundle (semu_app.nix) re-wraps
# it against the bundle's asset-staged lib/semu.
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
    makeWrapper $out/lib/semu/semu-btrc $out/bin/semu \
      --set SEMU_ASSET_ROOT $out/lib/semu \
      --set SEMU_BIN $out/bin/semu \
      --prefix PATH : ${lib.escapeShellArg runtimePath}
  '';
  meta = {
    description = "Semu BTRC runtime CLI";
    license = lib.licenses.mit;
  };
}
