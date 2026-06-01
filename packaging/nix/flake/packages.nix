{ pkgs, btrc, system }:

let
  lib = pkgs.lib;
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

  retroarch =
    if isX86Linux then
      pkgs.retroarch.withCores (cores: [
        cores.gambatte
        cores.mgba
        cores.genesis-plus-gx
        cores.snes9x
        cores.mesen
        cores.mupen64plus
        cores.desmume
        cores.beetle-psx
        cores.beetle-psx-hw
        cores.ppsspp
        cores.flycast
        cores.dolphin
      ])
    else if isDarwin then pkgs.callPackage ../retroarch-mac.nix {}
    else null;

  pcsx2 =
    if isX86Linux then pkgs.pcsx2
    else if isDarwin then pkgs.callPackage ../pcsx2-mac.nix {}
    else null;

  cemu =
    if isX86Linux then pkgs.cemu
    else if isDarwin then pkgs.callPackage ../cemu-mac.nix {}
    else null;

  ppsspp = if isX86Linux then pkgs.ppsspp else null;

  flycast =
    if isX86Linux then
      pkgs.flycast.overrideAttrs (old: {
        postPatch = (old.postPatch or "") + ''
          substituteInPlace core/deps/glslang/SPIRV/SpvBuilder.h \
            --replace-fail '#include <unordered_map>' '#include <cstdint>
          #include <unordered_map>'
        '';
      })
    else null;

  gopher64 = if isX86Linux then pkgs.gopher64 else null;
  melonds = if isX86Linux then pkgs.melonds else null;

  azahar =
    if isDarwin then pkgs.callPackage ../azahar-mac.nix {}
    else if isX86Linux then pkgs.azahar
    else null;

  syncthingtray = if isLinux then pkgs.syncthingtray else null;
  bubblewrap = if isLinux then pkgs.bubblewrap else null;

  btrcpy = btrc.packages.${system}.btrcpy;

  semuCli = pkgs.callPackage ../semu-cli.nix {
    inherit (pkgs) syncthing curl;
    inherit syncthingtray bubblewrap;
  };

  routedEmulator = args: pkgs.callPackage ../routed-emulator.nix (args // {
    inherit semuCli;
  });

  routedEmulators =
    if isX86Linux then [
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
    ]
    else [];
in
{
  inherit btrcpy;
  semu-cli = semuCli;
} // lib.optionalAttrs isFullBundleTarget {
  dolphin = pkgs.dolphin-emu;
  ares = pkgs.ares;
  inherit azahar es-de retroarch pcsx2 cemu;
} // lib.optionalAttrs (ryujinx != null && isFullBundleTarget) {
  inherit ryujinx;
} // lib.optionalAttrs isX86Linux {
  inherit ppsspp flycast gopher64 melonds;
  es-de-steamdeck = pkgs.callPackage ../es-de.nix { steamDeck = true; };
} // {
  default =
    if isFullBundleTarget then
      pkgs.callPackage ../semu.nix {
        inherit (pkgs) dolphin-emu ares;
        inherit azahar pcsx2 cemu ppsspp flycast gopher64 melonds ryujinx es-de syncthingtray bubblewrap;
        inherit (pkgs) syncthing curl;
        inherit semuCli routedEmulators;
        retroarch-bare = retroarch;
      }
    else semuCli;
} // lib.optionalAttrs isX86Linux {
  semu-retroarch = builtins.elemAt routedEmulators 0;
  semu-dolphin = builtins.elemAt routedEmulators 1;
  semu-ppsspp = builtins.elemAt routedEmulators 2;
  semu-flycast = builtins.elemAt routedEmulators 3;
  semu-gopher64 = builtins.elemAt routedEmulators 4;
  semu-melonds = builtins.elemAt routedEmulators 5;
  semu-pcsx2 = builtins.elemAt routedEmulators 6;
  semu-cemu = builtins.elemAt routedEmulators 7;
  semu-azahar = builtins.elemAt routedEmulators 8;
  semu-ryujinx = builtins.elemAt routedEmulators 9;
  semu-es-de = builtins.elemAt routedEmulators 10;
  semu-routed-emulators = pkgs.symlinkJoin {
    name = "semu-routed-emulators";
    paths = routedEmulators;
  };
}
