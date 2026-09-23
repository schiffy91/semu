# Compile the BTRC program on its own; runtime data is a separate package.
{ lib, stdenv, btrcpy }:

let
  repositoryRoot = ../..;
  programSource = lib.fileset.toSource {
    root = repositoryRoot;
    fileset = lib.fileset.fileFilter (file: file.hasExt "btrc" || file.hasExt "h") (repositoryRoot + "/src");
  };
in
stdenv.mkDerivation {
  pname = "semu-program";
  version = "0.2.0";
  src = programSource;
  nativeBuildInputs = [ btrcpy ];
  dontConfigure = true;

  buildPhase = ''
    btrcpy src/semu.btrc -o semu.c --strict-imports --no-cache --no-stdlib
    $CC semu.c -std=c11 -O2 -Isrc/launch -o semu-btrc -lm
  '';

  installPhase = ''
    install -Dm755 semu-btrc "$out/lib/semu/semu-btrc"
  '';

  meta = {
    description = "Compiled Semu BTRC program";
    license = lib.licenses.mit;
  };
}
