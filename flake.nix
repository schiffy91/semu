{
  description = "Semu — deterministic emulation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    btrc = {
      url = "github:schiffy91/btrc";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, btrc }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
      mkPkgs = system: import nixpkgs {
        inherit system;
        config.allowUnfreePredicate = pkg:
          nixpkgs.lib.hasPrefix "libretro-" (nixpkgs.lib.getName pkg);
      };
    in {
      packages = forAllSystems (system:
        import ./packaging/nix/flake/packages.nix {
          pkgs = mkPkgs system;
          inherit btrc system;
        });

      apps = forAllSystems (system:
        import ./packaging/nix/flake/apps.nix {
          pkgs = mkPkgs system;
          inherit self system;
        });

      checks = forAllSystems (system:
        import ./packaging/nix/flake/checks.nix {
          pkgs = mkPkgs system;
          inherit system;
        });

      nixosModules.default = import ./packaging/nix/module.nix;

      devShells = forAllSystems (system:
        import ./packaging/nix/flake/dev-shells.nix {
          pkgs = mkPkgs system;
          inherit btrc system;
        });
    };
}
