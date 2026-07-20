{
  description = "Semu — deterministic emulation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    # ES-DE 3.4.0 still requires FreeImage. Current nixpkgs removed that
    # dependency, so keep its final packaged toolchain isolated to ES-DE.
    nixpkgsEsDe.url = "github:NixOS/nixpkgs/ac62194c3917d5f474c1a844b6fd6da2db95077d";
    btrc = {
      url = "github:schiffy91/btrc/codex/production-readiness";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    nixGL.url = "github:nix-community/nixGL";
  };

  # All recipes live in packaging/nix/; this file only wires inputs
  # into the flake/ helper functions.
  outputs =
    {
      self,
      nixpkgs,
      nixpkgsEsDe,
      btrc,
      nixGL,
    }:
    let
      systems = [
        "aarch64-darwin"
        "x86_64-darwin"
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
      mkPkgs =
        system:
        import nixpkgs {
          inherit system;
          config.allowUnfreePredicate = pkg: nixpkgs.lib.hasPrefix "libretro-" (nixpkgs.lib.getName pkg);
        };
      shared = {
        inherit
          self
          nixpkgs
          nixpkgsEsDe
          btrc
          nixGL
          systems
          forAllSystems
          mkPkgs
          ;
      };
    in
    {
      packages = import ./packaging/nix/flake/packages.nix shared;
      apps = import ./packaging/nix/flake/apps.nix shared;
      checks = import ./packaging/nix/flake/checks.nix shared;
      devShells = import ./packaging/nix/flake/dev-shells.nix shared;
    };
}
