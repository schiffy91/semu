{
  description = "Schemulator — deterministic emulation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    systems = [ "aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux" ];
    forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f system);
  in {
    packages = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
      isLinux = pkgs.stdenv.hostPlatform.isLinux;
      isDarwin = pkgs.stdenv.hostPlatform.isDarwin;
      ryujinx = pkgs.callPackage ./nix/ryujinx.nix {};
      es-de = pkgs.callPackage ./nix/es-de.nix {};
      retroarch = if isLinux then
        (pkgs.retroarch.withCores (cores: with cores; [
          gambatte       # gb, gbc
          mgba           # gba
          genesis-plus-gx # genesis
          snes9x         # snes
          mesen          # nes
          mupen64plus    # n64
          desmume        # nds
          beetle-psx     # psx (default)
          beetle-psx-hw  # psx (hardware accelerated)
          ppsspp         # psp
          flycast        # dreamcast
          dolphin        # gc, wii (alternative to standalone)
        ]))
      else pkgs.callPackage ./nix/retroarch-mac.nix {};
      pcsx2 = if isLinux then pkgs.pcsx2
              else pkgs.callPackage ./nix/pcsx2-mac.nix {};
      cemu = if isLinux then pkgs.cemu
             else pkgs.callPackage ./nix/cemu-mac.nix {};
    in {
      # --- Individual emulators (all platforms) ---
      dolphin = pkgs.dolphin-emu;
      azahar = pkgs.azahar;
      inherit ryujinx es-de retroarch pcsx2 cemu;
    } // pkgs.lib.optionalAttrs isLinux {
      es-de-steamdeck = pkgs.callPackage ./nix/es-de.nix { steamDeck = true; };
    } // {
      # --- Unified bundle ---
      default = pkgs.callPackage ./nix/schemulator.nix {
        inherit (pkgs) dolphin-emu azahar;
        inherit pcsx2 cemu ryujinx es-de;
        retroarch-bare = retroarch;
      };
    });

    # `nix run` launches schemulator CLI
    apps = forAllSystems (system: {
      default = {
        type = "app";
        program = "${self.packages.${system}.default}/bin/schemulator";
      };
    });

    # NixOS module
    nixosModules.default = import ./nix/module.nix;

    # Dev shell
    devShells = forAllSystems (system: let
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      default = pkgs.mkShell {
        buildInputs = [
          (pkgs.python3.withPackages (ps: [
            ps.pycryptodome
            ps.pytest
          ]))
        ];
      };
    });
  };
}
