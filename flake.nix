{
  description = "Schemulator — deterministic emulation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    btrc = {
      url = "github:schiffy91/btrc";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, btrc }: let
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
      isDarwin = pkgs.stdenv.hostPlatform.isDarwin;
      ryujinx = pkgs.ryubing;
      es-de = pkgs.callPackage ./nix/es-de.nix {};
      retroarch = if isLinux then
        (pkgs.retroarch.withCores (cores: [
          cores.gambatte       # gb, gbc
          cores.mgba           # gba
          cores.genesis-plus-gx # genesis
          cores.snes9x         # snes
          cores.mesen          # nes
          cores.mupen64plus    # n64
          cores.desmume        # nds
          cores.beetle-psx     # psx (default)
          cores.beetle-psx-hw  # psx (hardware accelerated)
          cores.ppsspp         # psp
          cores.flycast        # dreamcast
          cores.dolphin        # gc, wii (alternative to standalone)
        ]))
      else pkgs.callPackage ./nix/retroarch-mac.nix {};
      pcsx2 = if isLinux then pkgs.pcsx2
              else pkgs.callPackage ./nix/pcsx2-mac.nix {};
      cemu = if isLinux then pkgs.cemu
             else pkgs.callPackage ./nix/cemu-mac.nix {};
      ppsspp = if isLinux then pkgs.ppsspp else null;
      flycast = if isLinux then pkgs.flycast else null;
      gopher64 = if isLinux then pkgs.gopher64 else null;
      melonds = if isLinux then pkgs.melonds else null;
      azahar = if isDarwin then pkgs.callPackage ./nix/azahar-mac.nix {}
               else pkgs.azahar;
      syncthingtray = if isLinux then pkgs.syncthingtray else null;
      bubblewrap = if isLinux then pkgs.bubblewrap else null;
      btrcpy = btrc.packages.${system}.btrcpy;
      schemulatorCli = pkgs.callPackage ./nix/schemulator-cli.nix {
        inherit (pkgs) syncthing curl;
        inherit syncthingtray bubblewrap;
      };
      routedEmulator = args: pkgs.callPackage ./nix/routed-emulator.nix (args // {
        inherit schemulatorCli;
      });
      routedEmulators = if isLinux then [
        (routedEmulator {
          emulatorName = "retroarch";
          emulatorPackage = retroarch;
          executableName = "retroarch";
        })
        (routedEmulator {
          emulatorName = "dolphin";
          emulatorPackage = pkgs.dolphin-emu;
          executableName = "dolphin-emu";
        })
        (routedEmulator {
          emulatorName = "ppsspp";
          emulatorPackage = ppsspp;
        })
        (routedEmulator {
          emulatorName = "flycast";
          emulatorPackage = flycast;
        })
        (routedEmulator {
          emulatorName = "gopher64";
          emulatorPackage = gopher64;
        })
        (routedEmulator {
          emulatorName = "melonds";
          emulatorPackage = melonds;
          executableName = "melonDS";
        })
        (routedEmulator {
          emulatorName = "pcsx2";
          emulatorPackage = pcsx2;
        })
        (routedEmulator {
          emulatorName = "cemu";
          emulatorPackage = cemu;
        })
        (routedEmulator {
          emulatorName = "azahar";
          emulatorPackage = azahar;
        })
        (routedEmulator {
          emulatorName = "ryujinx";
          emulatorPackage = ryujinx;
        })
        (routedEmulator {
          emulatorName = "es-de";
          emulatorPackage = es-de;
          executableName = "es-de";
        })
      ] else [];
    in {
      # --- Individual emulators (all platforms) ---
      inherit btrcpy;
      dolphin = pkgs.dolphin-emu;
      ares = pkgs.ares;
      inherit azahar ryujinx es-de retroarch pcsx2 cemu;
    } // pkgs.lib.optionalAttrs isLinux {
      inherit ppsspp flycast gopher64 melonds;
      es-de-steamdeck = pkgs.callPackage ./nix/es-de.nix { steamDeck = true; };
    } // {
      # --- Unified bundle ---
      default = pkgs.callPackage ./nix/schemulator.nix {
        inherit (pkgs) dolphin-emu ares;
        inherit azahar pcsx2 cemu ppsspp flycast gopher64 melonds ryujinx es-de syncthingtray bubblewrap;
        inherit (pkgs) syncthing curl;
        inherit schemulatorCli routedEmulators;
        retroarch-bare = retroarch;
      };
    } // pkgs.lib.optionalAttrs isLinux {
      schem-retroarch = builtins.elemAt routedEmulators 0;
      schem-dolphin = builtins.elemAt routedEmulators 1;
      schem-ppsspp = builtins.elemAt routedEmulators 2;
      schem-flycast = builtins.elemAt routedEmulators 3;
      schem-gopher64 = builtins.elemAt routedEmulators 4;
      schem-melonds = builtins.elemAt routedEmulators 5;
      schem-pcsx2 = builtins.elemAt routedEmulators 6;
      schem-cemu = builtins.elemAt routedEmulators 7;
      schem-azahar = builtins.elemAt routedEmulators 8;
      schem-ryujinx = builtins.elemAt routedEmulators 9;
      schem-es-de = builtins.elemAt routedEmulators 10;
      schem-routed-emulators = pkgs.symlinkJoin {
        name = "schemulator-routed-emulators";
        paths = routedEmulators;
      };
    });

    # `nix run` launches schemulator CLI
    apps = forAllSystems (system: let
      pkgs = mkPkgs system;
      isLinux = pkgs.stdenv.hostPlatform.isLinux;
      emulatorApp = name: {
        type = "app";
        program = "${self.packages.${system}.${name}}/bin/${name}";
      };
    in {
      default = {
        type = "app";
        program = "${self.packages.${system}.default}/bin/schemulator";
      };
    } // pkgs.lib.optionalAttrs isLinux {
      schem-retroarch = emulatorApp "schem-retroarch";
      schem-dolphin = emulatorApp "schem-dolphin";
      schem-ppsspp = emulatorApp "schem-ppsspp";
      schem-flycast = emulatorApp "schem-flycast";
      schem-gopher64 = emulatorApp "schem-gopher64";
      schem-melonds = emulatorApp "schem-melonds";
      schem-pcsx2 = emulatorApp "schem-pcsx2";
      schem-cemu = emulatorApp "schem-cemu";
      schem-azahar = emulatorApp "schem-azahar";
      schem-ryujinx = emulatorApp "schem-ryujinx";
      schem-es-de = emulatorApp "schem-es-de";
    });

    checks = forAllSystems (system: let
      pkgs = mkPkgs system;
      lib = pkgs.lib;
      schemulatorCli = pkgs.callPackage ./nix/schemulator-cli.nix {
        syncthing = null;
        syncthingtray = null;
        curl = null;
        bubblewrap = null;
      };
      routedEmulatorMatrix = [
        {
          emulatorName = "retroarch";
          commandName = "retroarch";
          executableName = "retroarch";
          seedScript = ''
            mkdir -p "$project/RetroArch/config"
            printf 'seed input\n' > "$project/RetroArch/config/input.cfg"
            printf 'seed retroarch\n' > "$project/RetroArch/retroarch.cfg"
          '';
          stateAssertions = ''
            grep -F 'ARG=--config' "$capture"
            grep -F "ARG=$state/config/retroarch/retroarch.cfg" "$capture"
            test -f "$state/config/retroarch/input.cfg"
            test -f "$state/config/retroarch/retroarch.cfg"
            test ! -L "$state/config/retroarch/input.cfg"
            test ! -L "$state/config/retroarch/retroarch.cfg"
          '';
        }
        {
          emulatorName = "dolphin";
          commandName = "dolphin-emu";
          executableName = "dolphin-emu";
          seedScript = ''
            mkdir -p "$project/Dolphin/config" "$project/Dolphin/data"
            printf 'seed dolphin config\n' > "$project/Dolphin/config/settings.ini"
            printf 'seed dolphin data\n' > "$project/Dolphin/data/memory-card.raw"
          '';
          stateAssertions = ''
            test -f "$state/config/dolphin-emu/settings.ini"
            test -f "$state/data/dolphin-emu/memory-card.raw"
            test ! -L "$state/config/dolphin-emu/settings.ini"
            test ! -L "$state/data/dolphin-emu/memory-card.raw"
          '';
        }
        {
          emulatorName = "ppsspp";
          commandName = "ppsspp";
          seedScript = ''
            mkdir -p "$project/PPSSPP/config" "$project/PPSSPP/data"
            printf 'seed ppsspp config\n' > "$project/PPSSPP/config/ppsspp.ini"
            printf 'seed ppsspp data\n' > "$project/PPSSPP/data/save.bin"
          '';
          stateAssertions = ''
            test -f "$state/config/ppsspp/ppsspp.ini"
            test -f "$state/data/ppsspp/save.bin"
            test ! -L "$state/config/ppsspp/ppsspp.ini"
            test ! -L "$state/data/ppsspp/save.bin"
          '';
        }
        {
          emulatorName = "flycast";
          commandName = "flycast";
          seedScript = ''
            mkdir -p "$project/Flycast/config" "$project/Flycast/data"
            printf 'seed flycast config\n' > "$project/Flycast/config/emu.cfg"
            printf 'seed flycast data\n' > "$project/Flycast/data/flash.bin"
          '';
          stateAssertions = ''
            test -f "$state/config/flycast/emu.cfg"
            test -f "$state/data/flycast/flash.bin"
            test ! -L "$state/config/flycast/emu.cfg"
            test ! -L "$state/data/flycast/flash.bin"
          '';
        }
        {
          emulatorName = "gopher64";
          commandName = "gopher64";
          seedScript = ''
            mkdir -p "$project/Gopher64/config"
            printf 'seed gopher64 config\n' > "$project/Gopher64/config/settings.toml"
          '';
          stateAssertions = ''
            test -f "$state/config/gopher64/settings.toml"
            test ! -L "$state/config/gopher64/settings.toml"
          '';
        }
        {
          emulatorName = "melonds";
          commandName = "melonDS";
          executableName = "melonDS";
          seedScript = ''
            mkdir -p "$project/melonDS/config" "$project/melonDS/data"
            printf 'seed melonds config\n' > "$project/melonDS/config/melonDS.ini"
            printf 'seed melonds data\n' > "$project/melonDS/data/firmware.bin"
          '';
          stateAssertions = ''
            test -f "$state/config/melonDS/melonDS.ini"
            test -f "$state/data/melonDS/firmware.bin"
            test ! -L "$state/config/melonDS/melonDS.ini"
            test ! -L "$state/data/melonDS/firmware.bin"
          '';
        }
        {
          emulatorName = "pcsx2";
          commandName = "pcsx2-qt";
          seedScript = ''
            mkdir -p "$project/PCSX2/config"
            printf 'seed pcsx2 config\n' > "$project/PCSX2/config/PCSX2.ini"
          '';
          stateAssertions = ''
            test -f "$state/config/PCSX2/PCSX2.ini"
            test ! -L "$state/config/PCSX2/PCSX2.ini"
          '';
        }
        {
          emulatorName = "cemu";
          commandName = "cemu";
          seedScript = ''
            mkdir -p "$project/Cemu/config" "$project/Cemu/data"
            printf 'seed cemu config\n' > "$project/Cemu/config/settings.xml"
            printf 'seed cemu data\n' > "$project/Cemu/data/mlc.bin"
          '';
          stateAssertions = ''
            test -f "$state/config/Cemu/settings.xml"
            test -f "$state/data/Cemu/mlc.bin"
            test ! -L "$state/config/Cemu/settings.xml"
            test ! -L "$state/data/Cemu/mlc.bin"
          '';
        }
        {
          emulatorName = "azahar";
          commandName = "azahar";
          seedScript = ''
            mkdir -p "$project/Azahar/data"
            printf 'seed azahar data\n' > "$project/Azahar/data/qt-config.ini"
          '';
          stateAssertions = ''
            test -f "$state/data/azahar-emu/qt-config.ini"
            test ! -L "$state/data/azahar-emu/qt-config.ini"
          '';
        }
        {
          emulatorName = "ryujinx";
          commandName = "Ryujinx";
          seedScript = ''
            mkdir -p "$project/Ryujinx/config"
            printf 'seed ryujinx config\n' > "$project/Ryujinx/config/Config.json"
          '';
          stateAssertions = ''
            test -f "$state/config/Ryujinx/Config.json"
            test ! -L "$state/config/Ryujinx/Config.json"
          '';
        }
      ];
      mkMockEmulator = commandName: pkgs.writeShellApplication {
        name = commandName;
        text = ''
          : "''${SCHEMULATOR_CAPTURE:?}"
          {
            printf 'COMMAND=%s\n' "''${0##*/}"
            printf 'SCHEMULATOR_PROJECT_DIR=%s\n' "$SCHEMULATOR_PROJECT_DIR"
            printf 'SCHEMULATOR_ROMS_DIR=%s\n' "$SCHEMULATOR_ROMS_DIR"
            printf 'HOME=%s\n' "$HOME"
            printf 'XDG_CONFIG_HOME=%s\n' "$XDG_CONFIG_HOME"
            printf 'XDG_DATA_HOME=%s\n' "$XDG_DATA_HOME"
            printf 'XDG_CACHE_HOME=%s\n' "$XDG_CACHE_HOME"
            for arg in "$@"; do
              printf 'ARG=%s\n' "$arg"
            done
          } > "$SCHEMULATOR_CAPTURE"
        '';
      };
      mkMockRoutedEmulator = spec:
        pkgs.callPackage ./nix/routed-emulator.nix ({
          inherit (spec) emulatorName;
          emulatorPackage = mkMockEmulator spec.commandName;
          inherit schemulatorCli;
        } // lib.optionalAttrs (spec ? executableName) {
          inherit (spec) executableName;
        });
      mkRoutedEmulatorCheck = spec: let
        mockRoutedEmulator = mkMockRoutedEmulator spec;
      in lib.nameValuePair "routed-emulator-${spec.emulatorName}-mock" (pkgs.runCommand "schemulator-routed-emulator-${spec.emulatorName}-mock-check" {
        nativeBuildInputs = [ pkgs.coreutils pkgs.gnugrep ];
      } ''
        project="$TMPDIR/project-${spec.emulatorName}"
        capture="$TMPDIR/${spec.emulatorName}.capture"
        mkdir -p "$project"
        ${spec.seedScript}

        SCHEMULATOR_PROJECT_DIR="$project" \
        SCHEMULATOR_CAPTURE="$capture" \
        ${mockRoutedEmulator}/bin/schem-${spec.emulatorName} \
          "roms/${spec.emulatorName} game.rom" \
          "--sentinel=${spec.emulatorName}"

        state="$project/.schemulator/appimage-state/${spec.emulatorName}"
        grep -F "COMMAND=${spec.commandName}" "$capture"
        grep -F "SCHEMULATOR_PROJECT_DIR=$project" "$capture"
        grep -F "SCHEMULATOR_ROMS_DIR=$project/ES-DE/ES-DE/ROMs" "$capture"
        grep -F "HOME=$state/home" "$capture"
        grep -F "XDG_CONFIG_HOME=$state/config" "$capture"
        grep -F "XDG_DATA_HOME=$state/data" "$capture"
        grep -F "XDG_CACHE_HOME=$state/cache" "$capture"
        grep -F "ARG=roms/${spec.emulatorName} game.rom" "$capture"
        grep -F "ARG=--sentinel=${spec.emulatorName}" "$capture"
        ${spec.stateAssertions}

        touch "$out"
      '');
      routedEmulatorChecks = lib.listToAttrs (map mkRoutedEmulatorCheck routedEmulatorMatrix);
    in routedEmulatorChecks // {
      routed-emulator-mock = pkgs.runCommand "schemulator-routed-emulator-mock-check" {} ''
        ${lib.concatMapStringsSep "\n" (check: "test -e ${check}") (lib.attrValues routedEmulatorChecks)}
        touch "$out"
      '';
    });

    # NixOS module
    nixosModules.default = import ./nix/module.nix;

    # Dev shell
    devShells = forAllSystems (system: let
      pkgs = mkPkgs system;
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
