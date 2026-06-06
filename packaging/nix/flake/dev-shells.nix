{ self, nixpkgs, btrc, systems, forAllSystems, mkPkgs, ... }:

forAllSystems (system: let
      pkgs = mkPkgs system;
    in {
      default = pkgs.mkShell {
        buildInputs = [
          pkgs.bash
          pkgs.gnumake
          pkgs.stdenv.cc
          pkgs.ripgrep
        ];
      };
    })
