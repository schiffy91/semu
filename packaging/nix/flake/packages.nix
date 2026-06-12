{ self, nixpkgs, btrc, nixGL ? null, systems, forAllSystems, mkPkgs }:

forAllSystems (system: let
      pkgs = mkPkgs system;
      isLinux = pkgs.stdenv.hostPlatform.isLinux;
      isDarwin = pkgs.stdenv.hostPlatform.isDarwin;
      isX86Linux = system == "x86_64-linux";
      isFullBundleTarget = isDarwin || isX86Linux;
      ryujinx =
        if system == "x86_64-darwin" then null
        else pkgs.ryubing;
      es-de =
        if isFullBundleTarget then pkgs.callPackage ../es-de.nix {}
        else null;
      retroarch = if isX86Linux then
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
      else if isDarwin then pkgs.callPackage ../retroarch-mac.nix {}
      else null;
      pcsx2 = if isX86Linux then pkgs.pcsx2
              else if isDarwin then pkgs.callPackage ../pcsx2-mac.nix {}
              else null;
      cemu = if isX86Linux then pkgs.cemu
             else if isDarwin then pkgs.callPackage ../cemu-mac.nix {}
             else null;
      ppsspp = if isX86Linux then pkgs.ppsspp else null;
      flycast = if isX86Linux then pkgs.flycast.overrideAttrs (old: {
        postPatch = (old.postPatch or "") + ''
          substituteInPlace core/deps/glslang/SPIRV/SpvBuilder.h \
            --replace-fail '#include <unordered_map>' '#include <cstdint>
          #include <unordered_map>'
        '';
      }) else null;
      melonds = if isX86Linux then pkgs.melonds else null;
      azahar = if isDarwin then pkgs.callPackage ../azahar-mac.nix {}
               else if isX86Linux then pkgs.azahar.overrideAttrs (old: {
                 cmakeFlags = (old.cmakeFlags or []) ++ [
                   (pkgs.lib.cmakeBool "ENABLE_VULKAN" false)
                 ];
               })
               else null;
      syncthingtray = if isLinux then pkgs.syncthingtray else null;
      bubblewrap = if isLinux then pkgs.bubblewrap else null;
      nixGLIntel =
        if isX86Linux && nixGL != null
        then nixGL.packages.${system}.nixGLIntel
        else null;
      btrcpy = btrc.packages.${system}.btrcpy;
      semuCli = pkgs.callPackage ../semu-cli.nix {
        inherit (pkgs) syncthing curl;
        inherit syncthingtray bubblewrap;
      };
      semuVisualAssets = pkgs.callPackage ../visual-assets.nix {};
      routedEmulator = args: pkgs.callPackage ../routed-emulator.nix (args // {
        inherit semuCli;
      });
      routedEmulators = if isX86Linux then [
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
      # --- Core Semu tooling (all platforms) ---
      inherit btrcpy;
      semu-cli = semuCli;
      semu-visual-assets = semuVisualAssets;
    } // pkgs.lib.optionalAttrs isFullBundleTarget {
      # --- Individual emulators (supported desktop/Deck platforms) ---
      dolphin = pkgs.dolphin-emu;
      ares = pkgs.ares;
      inherit azahar es-de retroarch pcsx2 cemu;
    } // pkgs.lib.optionalAttrs (ryujinx != null && isFullBundleTarget) {
      inherit ryujinx;
    } // pkgs.lib.optionalAttrs isX86Linux {
      inherit ppsspp flycast melonds;
      es-de-steamdeck = pkgs.callPackage ../es-de.nix { steamDeck = true; };
    } // {
      # --- Unified bundle ---
      default = if isFullBundleTarget then pkgs.callPackage ../semu.nix {
        inherit (pkgs) dolphin-emu ares;
        inherit azahar pcsx2 cemu ppsspp flycast melonds ryujinx es-de syncthingtray bubblewrap nixGLIntel;
        inherit (pkgs) syncthing curl libretro-shaders-slang;
        inherit semuCli semuVisualAssets routedEmulators;
        retroarch-bare = retroarch;
      } else semuCli;
    } // pkgs.lib.optionalAttrs isX86Linux {
      semu-retroarch = builtins.elemAt routedEmulators 0;
      semu-dolphin = builtins.elemAt routedEmulators 1;
      semu-ppsspp = builtins.elemAt routedEmulators 2;
      semu-flycast = builtins.elemAt routedEmulators 3;
      semu-melonds = builtins.elemAt routedEmulators 4;
      semu-pcsx2 = builtins.elemAt routedEmulators 5;
      semu-cemu = builtins.elemAt routedEmulators 6;
      semu-azahar = builtins.elemAt routedEmulators 7;
      semu-ryujinx = builtins.elemAt routedEmulators 8;
      semu-es-de = builtins.elemAt routedEmulators 9;
      semu-nixgl = nixGLIntel;
      semu-routed-emulators = pkgs.symlinkJoin {
        name = "semu-routed-emulators";
        paths = routedEmulators ++ pkgs.lib.optional (nixGLIntel != null) nixGLIntel;
      };
    })
