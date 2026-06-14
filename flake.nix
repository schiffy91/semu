{
  description = "Semu — deterministic emulation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    nixpkgsFreeimage.url = "github:nixos/nixpkgs/93e8cdce7afc64297cfec447c311470788131cd9";
    btrc = {
      url = "github:schiffy91/btrc";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    nixGL.url = "github:nix-community/nixGL";
  };

  outputs = { self, nixpkgs, nixpkgsFreeimage, btrc, nixGL }: let
    systems = [ "aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux" ];
    forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
    mkPkgs = system: import nixpkgs {
      inherit system;
      config.allowUnfreePredicate = pkg:
        nixpkgs.lib.hasPrefix "libretro-" (nixpkgs.lib.getName pkg);
    };
    shared = { inherit self nixpkgs nixpkgsFreeimage btrc nixGL systems forAllSystems mkPkgs; };
  in {
    packages = import ./build/packaging/nix/flake/packages.nix shared;
    apps = import ./build/packaging/nix/flake/apps.nix shared;
    checks = import ./build/packaging/nix/flake/checks.nix shared;
    nixosModules.default = import ./build/packaging/nix/module.nix;
    devShells = import ./build/packaging/nix/flake/dev-shells.nix shared;
  };
}
