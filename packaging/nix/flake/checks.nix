{ self, nixpkgs, btrc, systems, forAllSystems, mkPkgs, ... }:

forAllSystems (system: let
      pkgs = mkPkgs system;
      lib = pkgs.lib;
      semuCli = pkgs.callPackage ../semu-cli.nix {
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
            mkdir -p "$project/emulators/profiles/RetroArch/config"
            printf 'seed input\n' > "$project/emulators/profiles/RetroArch/config/input.cfg"
            printf 'seed retroarch\n' > "$project/emulators/profiles/RetroArch/retroarch.cfg"
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
            mkdir -p "$project/emulators/profiles/Dolphin/config" "$project/emulators/profiles/Dolphin/data"
            printf 'seed dolphin config\n' > "$project/emulators/profiles/Dolphin/config/settings.ini"
            printf 'seed dolphin data\n' > "$project/emulators/profiles/Dolphin/data/memory-card.raw"
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
            mkdir -p "$project/emulators/profiles/PPSSPP/config" "$project/emulators/profiles/PPSSPP/data"
            printf 'seed ppsspp config\n' > "$project/emulators/profiles/PPSSPP/config/ppsspp.ini"
            printf 'seed ppsspp data\n' > "$project/emulators/profiles/PPSSPP/data/save.bin"
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
            mkdir -p "$project/emulators/profiles/Flycast/config" "$project/emulators/profiles/Flycast/data"
            printf 'seed flycast config\n' > "$project/emulators/profiles/Flycast/config/emu.cfg"
            printf 'seed flycast data\n' > "$project/emulators/profiles/Flycast/data/flash.bin"
          '';
          stateAssertions = ''
            test -f "$state/config/flycast/emu.cfg"
            test -f "$state/data/flycast/flash.bin"
            test ! -L "$state/config/flycast/emu.cfg"
            test ! -L "$state/data/flycast/flash.bin"
          '';
        }
        {
          emulatorName = "melonds";
          commandName = "melonDS";
          executableName = "melonDS";
          seedScript = ''
            mkdir -p "$project/emulators/profiles/melonDS/config" "$project/emulators/profiles/melonDS/data"
            printf 'seed melonds config\n' > "$project/emulators/profiles/melonDS/config/melonDS.ini"
            printf 'seed melonds data\n' > "$project/emulators/profiles/melonDS/data/firmware.bin"
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
            mkdir -p "$project/emulators/profiles/PCSX2/config"
            printf 'seed pcsx2 config\n' > "$project/emulators/profiles/PCSX2/config/PCSX2.ini"
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
            mkdir -p "$project/emulators/profiles/Cemu/config" "$project/emulators/profiles/Cemu/data"
            printf 'seed cemu config\n' > "$project/emulators/profiles/Cemu/config/settings.xml"
            printf 'seed cemu data\n' > "$project/emulators/profiles/Cemu/data/mlc.bin"
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
            mkdir -p "$project/emulators/profiles/Azahar/data"
            printf 'seed azahar data\n' > "$project/emulators/profiles/Azahar/data/qt-config.ini"
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
            mkdir -p "$project/emulators/profiles/Ryujinx/config"
            printf 'seed ryujinx config\n' > "$project/emulators/profiles/Ryujinx/config/Config.json"
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
          : "''${SEMU_CAPTURE:?}"
          {
            printf 'COMMAND=%s\n' "''${0##*/}"
            printf 'SEMU_PROJECT_DIR=%s\n' "$SEMU_PROJECT_DIR"
            printf 'SEMU_ROMS_DIR=%s\n' "$SEMU_ROMS_DIR"
            printf 'HOME=%s\n' "$HOME"
            printf 'XDG_CONFIG_HOME=%s\n' "$XDG_CONFIG_HOME"
            printf 'XDG_DATA_HOME=%s\n' "$XDG_DATA_HOME"
            printf 'XDG_CACHE_HOME=%s\n' "$XDG_CACHE_HOME"
            for arg in "$@"; do
              printf 'ARG=%s\n' "$arg"
            done
          } > "$SEMU_CAPTURE"
        '';
      };
      mkMockRoutedEmulator = spec:
        pkgs.callPackage ../routed-emulator.nix ({
          inherit (spec) emulatorName;
          emulatorPackage = mkMockEmulator spec.commandName;
          inherit semuCli;
        } // lib.optionalAttrs (spec ? executableName) {
          inherit (spec) executableName;
        });
      mkRoutedEmulatorCheck = spec: let
        mockRoutedEmulator = mkMockRoutedEmulator spec;
      in lib.nameValuePair "routed-emulator-${spec.emulatorName}-mock" (pkgs.runCommand "semu-routed-emulator-${spec.emulatorName}-mock-check" {
        nativeBuildInputs = [ pkgs.coreutils pkgs.gnugrep ];
      } ''
        project="$TMPDIR/project-${spec.emulatorName}"
        capture="$TMPDIR/${spec.emulatorName}.capture"
        mkdir -p "$project"
        ${spec.seedScript}

        SEMU_PROJECT_DIR="$project" \
        SEMU_CAPTURE="$capture" \
        ${mockRoutedEmulator}/bin/semu-${spec.emulatorName} \
          "roms/${spec.emulatorName} game.rom" \
          "--sentinel=${spec.emulatorName}"

        state="$project/.semu/appimage-state/${spec.emulatorName}"
        grep -F "COMMAND=${spec.commandName}" "$capture"
        grep -F "SEMU_PROJECT_DIR=$project" "$capture"
        grep -F "SEMU_ROMS_DIR=$project/ES-DE/ES-DE/ROMs" "$capture"
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
      routed-emulator-mock = pkgs.runCommand "semu-routed-emulator-mock-check" {} ''
        ${lib.concatMapStringsSep "\n" (check: "test -e ${check}") (lib.attrValues routedEmulatorChecks)}
        touch "$out"
      '';
    })
