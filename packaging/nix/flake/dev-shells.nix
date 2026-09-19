{ btrc, forAllSystems, mkPkgs, ... }:

forAllSystems (system:
  let pkgs = mkPkgs system;
  in {
    default = pkgs.mkShell {
      packages = [ btrc.packages.${system}.btrcpy pkgs.gnumake ];
    };
  })
