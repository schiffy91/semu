# Flake checks: every package evaluates and the contract tests pass.
{ self, btrc, forAllSystems, mkPkgs, ... }:

forAllSystems (system:
  let
    pkgs = mkPkgs system;
    packages = self.packages.${system};
    contracts = pkgs.stdenv.mkDerivation {
      name = "semu-contracts";
      src = pkgs.lib.fileset.toSource {
        root = ../../..;
        fileset = pkgs.lib.fileset.unions [ ../../../src ../../../tests/contracts ../../../config ];
      };
      nativeBuildInputs = [ packages.btrcpy ];
      dontConfigure = true;
      buildPhase = ''
        btrcpy tests/contracts/main.btrc -o contracts.c --strict-imports --no-cache --no-stdlib
        $CC contracts.c -std=c11 -O1 -o contracts -lm
      '';
      installPhase = ''
        export HOME="$TMPDIR/home"
        mkdir -p "$HOME"
        SEMU_PROJECT="$PWD" ./contracts
        touch "$out"
      '';
    };
  in {
    inherit contracts;
    semu = packages.semu;
    retroarch = packages.retroarch;
    es-de = packages.es-de;
  })
