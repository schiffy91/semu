# flake/dev-shells.nix — the shell `nix develop --command make btrc-build`
# needs on a bare host (Deck bootstrap): btrcpy, make, perl; mkShell's stdenv
# provides cc.
{ btrc, forAllSystems, mkPkgs, ... }:

forAllSystems (system: let
  pkgs = mkPkgs system;
in {
  default = pkgs.mkShell {
    packages = [
      btrc.packages.${system}.btrcpy
      pkgs.gnumake
      pkgs.perl
    ];
  };
})
