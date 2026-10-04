# Compile the BTRC program on its own; runtime data is a separate package.
{ lib, stdenv, btrcpy, libxcb }:

let
  repositoryRoot = ../..;
  programSource = lib.fileset.toSource {
    root = repositoryRoot;
    fileset = lib.fileset.fileFilter (file: file.hasExt "btrc" || file.hasExt "h") (repositoryRoot + "/src");
  };
  # Linux: the supervisor's optional X key adapter dlopens libxcb from its store path, which also
  # keeps it in the closure (the same libxcb Mesa already ships). macOS keeps the bare sonames,
  # which do not load there, so the adapter stays idle.
  xcbDefines = lib.optionalString stdenv.hostPlatform.isLinux
    "-DSEMU_XCB_LIBRARY='\"${libxcb}/lib/libxcb.so.1\"' -DSEMU_XCB_INPUT_LIBRARY='\"${libxcb}/lib/libxcb-xinput.so.0\"' -DSEMU_XCB_TEST_LIBRARY='\"${libxcb}/lib/libxcb-xtest.so.0\"'";
in
stdenv.mkDerivation {
  pname = "semu-program";
  version = "0.2.0";
  src = programSource;
  nativeBuildInputs = [ btrcpy ];
  dontConfigure = true;

  buildPhase = ''
    btrcpy src/semu.btrc -o semu.c --strict-imports --no-cache --no-stdlib
    $CC semu.c -std=c11 -O2 -Isrc/launch ${xcbDefines} -o semu-btrc -lm
  '';

  installPhase = ''
    install -Dm755 semu-btrc "$out/lib/semu/semu-btrc"
  '';

  meta = {
    description = "Compiled Semu BTRC program";
    license = lib.licenses.mit;
  };
}
