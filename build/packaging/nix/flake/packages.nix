{ self, nixpkgs, nixpkgsFreeimage, btrc, nixGL ? null, systems, forAllSystems, mkPkgs }:

forAllSystems (system: let
      pkgs = mkPkgs system;
      freeimagePkgs = import nixpkgsFreeimage {
        inherit system;
        config.permittedInsecurePackages = [
          "freeimage-3.18.0-unstable-2024-04-18"
        ];
      };
      isLinux = pkgs.stdenv.hostPlatform.isLinux;
      isDarwin = pkgs.stdenv.hostPlatform.isDarwin;
      isX86Linux = system == "x86_64-linux";
      isFullBundleTarget = isDarwin || isX86Linux;
      ryujinx =
        if system == "x86_64-darwin" then null
        else pkgs.callPackage ../../../../config/emulators/ryujinx/package.nix {
          inherit semuPackage;
        };
      es-de =
        if isFullBundleTarget then pkgs.callPackage ../es-de.nix {
          steamDeck = isX86Linux;
          freeimage = freeimagePkgs.freeimage;
        }
        else null;
      retroarch = if isX86Linux || isDarwin then pkgs.callPackage ../../../../config/emulators/retroarch/package.nix {
        inherit semuPackage;
      } else null;
      dolphin = if isX86Linux || isDarwin then pkgs.callPackage ../../../../config/emulators/dolphin/package.nix {
        inherit semuPackage;
      } else null;
      pcsx2 = if isX86Linux then pkgs.callPackage ../../../../config/emulators/pcsx2/package.nix {
        inherit semuPackage;
      }
              else if isDarwin then pkgs.callPackage ../pcsx2-mac.nix {}
              else null;
      cemu = if isX86Linux then pkgs.callPackage ../../../../config/emulators/cemu/package.nix {
        inherit semuPackage;
      }
             else if isDarwin then pkgs.callPackage ../cemu-mac.nix {}
             else null;
      ppsspp = if isX86Linux then pkgs.callPackage ../../../../config/emulators/ppsspp/package.nix {
        inherit semuPackage;
      } else null;
      flycast = if isX86Linux then pkgs.callPackage ../../../../config/emulators/flycast/package.nix {
        inherit semuPackage;
        extraAttrs = {
          postPatch = ''
            substituteInPlace core/deps/glslang/SPIRV/SpvBuilder.h \
              --replace-fail '#include <unordered_map>' '#include <cstdint>
            #include <unordered_map>'
          '';
        };
      } else null;
      melonds = if isX86Linux then pkgs.callPackage ../../../../config/emulators/melonds/package.nix {
        inherit semuPackage;
      } else null;
      azahar = if isDarwin then pkgs.callPackage ../azahar-mac.nix {}
               else if isX86Linux then (pkgs.callPackage ../../../../config/emulators/azahar/package.nix {
                 inherit semuPackage;
               }).overrideAttrs (old: {
                 cmakeFlags = (old.cmakeFlags or []) ++ [
                   (pkgs.lib.cmakeBool "ENABLE_VULKAN" false)
                 ];
               })
               else null;
      syncthingtray = if isLinux then pkgs.syncthingtray else null;
      bubblewrap = if isLinux then pkgs.bubblewrap else null;
      semuPackage = pkgs.callPackage ../lib/source-package.nix {};
      gamescope = if isX86Linux then pkgs.callPackage ../../../../config/compositors/gamescope/package.nix {
        inherit semuPackage;
      } else null;
      vkbasalt = if isX86Linux then pkgs.vkbasalt else null;
      vulkanLoader = if isX86Linux then pkgs.vulkan-loader else null;
      nixGLDefault =
        if isX86Linux && nixGL != null
        then nixGL.packages.${system}.nixGLDefault
        else null;
      btrcpy = btrc.packages.${system}.btrcpy;
      semuCli = pkgs.callPackage ../semu-cli.nix {
        inherit (pkgs) syncthing curl;
        inherit syncthingtray bubblewrap;
      };
      semuVisualAssets = pkgs.callPackage ../visual-assets.nix {};
      semuShaderBundle = if isFullBundleTarget then pkgs.callPackage ../shader-bundle.nix {
        inherit semuVisualAssets;
        inherit (pkgs) libretro-shaders-slang;
      } else null;
      routedEmulator = args: pkgs.callPackage ../routed-emulator.nix (args // {
        inherit semuCli;
        extraRuntimeInputs = pkgs.lib.optional (nixGLDefault != null) nixGLDefault;
      });
      routedEmulators = if isX86Linux then [
        (routedEmulator {
          emulatorName = "retroarch";
          emulatorPackage = retroarch;
          executableName = "retroarch";
        })
        (routedEmulator {
          emulatorName = "dolphin";
          emulatorPackage = dolphin;
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
      inherit dolphin;
      ares = pkgs.ares;
      semu-shader-bundle = semuShaderBundle;
      inherit azahar es-de retroarch pcsx2 cemu;
    } // pkgs.lib.optionalAttrs (ryujinx != null && isFullBundleTarget) {
      inherit ryujinx;
    } // pkgs.lib.optionalAttrs isX86Linux {
      inherit ppsspp flycast melonds;
      semu-gamescope = gamescope;
      es-de-steamdeck = pkgs.callPackage ../es-de.nix {
        steamDeck = true;
        freeimage = freeimagePkgs.freeimage;
      };
    } // {
      # --- Unified bundle ---
      default = if isFullBundleTarget then pkgs.callPackage ../semu.nix {
        dolphin-emu = dolphin;
        inherit (pkgs) ares;
        inherit azahar pcsx2 cemu ppsspp flycast melonds ryujinx es-de syncthingtray bubblewrap gamescope vkbasalt vulkanLoader nixGLDefault;
        inherit (pkgs) syncthing curl;
        inherit semuCli semuShaderBundle routedEmulators;
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
      semu-nixgl = nixGLDefault;
      semu-routed-emulators = pkgs.symlinkJoin {
        name = "semu-routed-emulators";
        paths = routedEmulators ++ pkgs.lib.optional (nixGLDefault != null) nixGLDefault;
      };
    })
