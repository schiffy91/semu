{
  description = "Semu — deterministic emulation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    btrc = {
      url = "github:schiffy91/btrc";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    nixGL.url = "github:nix-community/nixGL";
  };

  # All recipes live in src/semu/packaging/nix/; this file only wires inputs
  # into the flake/ helper functions.
  outputs = { self, nixpkgs, btrc, nixGL }: let
    systems = [ "aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux" ];
    forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
    mkPkgs = system: import nixpkgs {
      inherit system;
      config.allowUnfreePredicate = pkg:
        nixpkgs.lib.hasPrefix "libretro-" (nixpkgs.lib.getName pkg);
    };
    shared = { inherit self nixpkgs btrc nixGL systems forAllSystems mkPkgs; };
  in {
    packages = import ./src/semu/packaging/nix/flake/packages.nix shared;
    apps = import ./src/semu/packaging/nix/flake/apps.nix shared;
    checks = import ./src/semu/packaging/nix/flake/checks.nix shared;
    devShells = import ./src/semu/packaging/nix/flake/dev-shells.nix shared;
    nixosModules.default = import ./src/semu/packaging/nix/module.nix;
  };
}
