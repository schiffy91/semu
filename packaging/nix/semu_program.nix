# semu_program.nix - compile the BTRC program independently from runtime data.
{ lib, stdenv, buildPackages, btrcpy
, crossTarget ? null
, zig ? null
, requireStaticBootstrap ? false
}:

let
  staticBootstrapContract = {
    schema_version = 1;
    platform = "x86_64-linux";
    elf_class = "ELF64";
    machine = "x86-64";
    static = true;
    pt_interp = false;
  };
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
assert lib.assertMsg (
  !requireStaticBootstrap
  || (stdenv.hostPlatform.isLinux
    && stdenv.hostPlatform.isx86_64
    && stdenv.hostPlatform.isStatic)
) "the Steam Deck bootstrap CLI requires a static x86_64-linux stdenv";
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

  doInstallCheck = requireStaticBootstrap;
  installCheckPhase = lib.optionalString requireStaticBootstrap ''
    binary="$out/lib/semu/semu-btrc"
    readelf="${buildPackages.binutils-unwrapped}/bin/${stdenv.cc.targetPrefix}readelf"
    description="$(${buildPackages.file}/bin/file -b "$binary")"
    headers="$("$readelf" -hW "$binary")"
    segments="$("$readelf" -lW "$binary")"

    printf '%s\n' "$description" \
      | grep -Eq '^ELF 64-bit LSB (pie )?executable, x86-64,'
    printf '%s\n' "$description" | grep -Eq 'statically linked|static-pie linked'
    printf '%s\n' "$headers" | grep -Eq 'Class:[[:space:]]+ELF64$'
    printf '%s\n' "$headers" \
      | grep -Eq 'Machine:[[:space:]]+Advanced Micro Devices X86-64$'
    if printf '%s\n' "$segments" \
        | grep -Eq '(^|[[:space:]])INTERP([[:space:]]|$)'; then
      echo "Steam Deck bootstrap CLI contains a PT_INTERP loader" >&2
      exit 1
    fi
  '';

  passthru = lib.optionalAttrs requireStaticBootstrap {
    bootstrapContract = staticBootstrapContract;
  };

  meta = {
    description = "Compiled Semu BTRC program";
    license = lib.licenses.mit;
  };
}
