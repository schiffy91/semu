{ self, nixpkgs, btrc, systems, forAllSystems, mkPkgs, ... }:

forAllSystems (system: let
      pkgs = mkPkgs system;
    in {
      default = pkgs.mkShell {
        buildInputs = [
          btrc.packages.${system}.btrcpy
          pkgs.bash
          pkgs.gnumake
          pkgs.stdenv.cc
          pkgs.ripgrep
        ];
      };
    })
