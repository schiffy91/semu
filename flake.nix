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
    packages = import ./packaging/nix/flake/packages.nix shared;
    apps = import ./packaging/nix/flake/apps.nix shared;
    checks = import ./packaging/nix/flake/checks.nix shared;
    nixosModules.default = import ./packaging/nix/module.nix;
    devShells = import ./packaging/nix/flake/dev-shells.nix shared;
  };
}
