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

  # Wired directly against the approved packaging recipes under
  # src/semu/packaging/nix/ (semu_cli, semu_app, semu_emulators, semu_shaders,
  # semu_bezels — the tree contract's whitelist). The full emulator-bundle
  # outputs (per-emulator routed launchers, shader/bezel asset packs, the
  # Deck AppImage payload) are wired by the packaging phase on top of those
  # recipes; today the flake exposes the pieces the build gates need:
  # the btrcpy transpiler (make btrc-build runs `nix run .#btrcpy`) and the
  # semu CLI package.
  outputs = { self, nixpkgs, btrc, nixGL }: let
    systems = [ "aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux" ];
    forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
    mkPkgs = system: import nixpkgs {
      inherit system;
      config.allowUnfreePredicate = pkg:
        nixpkgs.lib.hasPrefix "libretro-" (nixpkgs.lib.getName pkg);
    };
  in {
    packages = forAllSystems (system: let
      pkgs = mkPkgs system;
      isLinux = pkgs.stdenv.hostPlatform.isLinux;
      btrcpy = btrc.packages.${system}.btrcpy;
    in {
      inherit btrcpy;
      default = pkgs.callPackage ./src/semu/packaging/nix/semu_cli.nix {
        inherit btrcpy;
        syncthingtray = if isLinux then pkgs.syncthingtray else null;
        bubblewrap = if isLinux then pkgs.bubblewrap else null;
      };
    });

    apps = forAllSystems (system: {
      btrcpy = {
        type = "app";
        program = "${self.packages.${system}.btrcpy}/bin/btrcpy";
      };
      default = {
        type = "app";
        program = "${self.packages.${system}.default}/bin/semu";
      };
    });

    devShells = forAllSystems (system: let
      pkgs = mkPkgs system;
    in {
      default = pkgs.mkShell {
        packages = [ self.packages.${system}.btrcpy ];
      };
    });
  };
}
