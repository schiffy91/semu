{
  description = "Schemulator — deterministic emulation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: let
    # Supported systems. Dropped:
    #   - aarch64-linux: Ryujinx wrapper has no verified ARM64 hash yet
    #     (round-3 critic finding #1).
    #   - x86_64-darwin: ES-DE x86_64-darwin source has no verified hash yet
    #     (round-5 critic finding #2). The flake is shippable on Apple Silicon
    #     Macs and x86_64 Linux. Re-add x86_64-darwin when nix/es-de.nix has
    #     a real hash.
    systems = [ "aarch64-darwin" "x86_64-linux" ];
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
      azahar = if isDarwin then pkgs.callPackage ./nix/azahar-mac.nix {}
               else pkgs.azahar;
    in {
      # --- Individual emulators (all platforms) ---
      dolphin = pkgs.dolphin-emu;
      ares = pkgs.ares;
      inherit azahar ryujinx es-de retroarch pcsx2 cemu;
    } // pkgs.lib.optionalAttrs isLinux {
      es-de-steamdeck = pkgs.callPackage ./nix/es-de.nix { steamDeck = true; };
    } // {
      # --- Unified bundle ---
      default = pkgs.callPackage ./nix/schemulator.nix {
        inherit (pkgs) dolphin-emu ares;
        inherit azahar pcsx2 cemu ryujinx es-de;
        retroarch-bare = retroarch;
      };

      # --- Desktop GUI (PySide6) ---
      # `nix run .#gui` launches the installer/updater UI.
      gui =
        let py = pkgs.python3.withPackages (ps: [ ps.pyside6 ]);
        in pkgs.writeShellScriptBin "schemulator-gui" ''
          export QT_QPA_PLATFORM=''${QT_QPA_PLATFORM:-xcb}
          cd ${./.} && exec ${py}/bin/python setup.py gui "$@"
        '';
    });

    # `nix run` launches schemulator CLI; `nix run .#gui` launches the GUI.
    apps = forAllSystems (system: {
      default = {
        type = "app";
        program = "${self.packages.${system}.default}/bin/schemulator";
      };
      gui = {
        type = "app";
        program = "${self.packages.${system}.gui}/bin/schemulator-gui";
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
