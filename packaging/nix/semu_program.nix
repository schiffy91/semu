# semu_program.nix - compile the BTRC program independently from runtime data.
{ lib, stdenv, btrcpy
, crossTarget ? null
, zig ? null
}:

let
  repositoryRoot = ../..;
  programSource = lib.cleanSourceWith {
    name = "semu-program-source";
    src = repositoryRoot;
    filter = path: type:
      let
        relative = lib.removePrefix "${toString repositoryRoot}/" (toString path);
        underProgram = lib.hasPrefix "src/" relative;
        contractTest = lib.hasSuffix "_contract_test.btrc" relative;
      in relative == "src"
        || (underProgram && type == "directory")
        || (underProgram
          && (lib.hasSuffix ".btrc" relative || lib.hasSuffix ".h" relative)
          && !contractTest);
  };
in
stdenv.mkDerivation {
  pname = "semu-program";
  version = "0.1.0";
  src = programSource;
  nativeBuildInputs = [ btrcpy ] ++ lib.optional (crossTarget != null) zig;
  dontBuild = true;

  installPhase = ''
    mkdir -p "$out/lib/semu"
    btrcpy src/main.btrc -o semu.c \
      --strict-imports --no-cache --no-stdlib
    ${if crossTarget == null
      then "${stdenv.cc.targetPrefix}cc"
      else "${zig}/bin/zig cc -target ${crossTarget}"} semu.c -std=c11 \
      -o "$out/lib/semu/semu-btrc" -lm
  '';

  meta = {
    description = "Compiled Semu BTRC program";
    license = lib.licenses.mit;
  };
}
