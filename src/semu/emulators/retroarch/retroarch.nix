# retroarch.nix — the RetroArch package recipes, owned next to the emulator
# contract. Both platform variants and every pin/hash live here;
# packaging/nix/semu_emulators.nix only discovers the inventory from the JSON
# contracts and injects the contract-derived `cores` list.
#
# macOS (the emulator.json macos nix backend): nixpkgs retroarch is broken on
# Darwin, so this packages the official universal DMG plus the libretro
# buildbot core zips. The zips come from the buildbot's rolling "latest"
# nightly directory, so those hashes go stale when the buildbot rotates;
# re-prefetch on mismatch (2026-07-01 pins).
#
# Linux: the Semu tap build. The sibling retroarch.patch makes
# video_driver_frame report the real content viewport + active state to the
# Semu compositor every frame (semu_tap_report_safe resolves at runtime, so
# an uncomposited run is a no-op). Source pin = flake.lock's nixpkgs
# retroarch-bare; wiring follows the proven retroarch-semu recipe (git
# history: packaging/nix/retroarch-semu.nix at 81dd8cc — patch + tap header
# + -DHAVE_SEMU_TAP, cross- or native-built for the Deck). The Deck's
# runtime flatpak RetroArch remains a runtime concern, not packaged here.
{ lib, stdenv, fetchurl, undmg, unzip, writeText, retroarch-bare, cores ? [ ] }:

let
  # Emulator-side reporting half of the tap contract (semu_tap.h). The
  # compositor (libsemutap) owns the other half; SEMU_TAP_ABI guards drift.
  semuTapHeader = writeText "semu_tap.h" ''
    // semu_tap.h — the Semu compositor "tap" interface (emulator reporting side).
    //
    // Each frame the emulator reports EXACTLY where the live game content sits in
    // the output framebuffer, and whether content is active at all. The Semu
    // compositor (loaded into the emulator process) uses ONLY this report — never
    // pixel inspection, never black-detection, never centering guesses.
    //
    //   Emulator each frame:  build SemuTapState -> semu_tap_report_safe(&s) -> present
    //
    // An un-tapped build is unaffected: semu_tap_report is resolved at runtime, so
    // when the compositor isn't loaded the call is a no-op.

    #ifndef SEMU_TAP_H
    #define SEMU_TAP_H

    #ifdef __cplusplus
    extern "C" {
    #endif

    #define SEMU_TAP_ABI 1

    // Coordinate origin of content_x/content_y within the framebuffer.
    enum {
        SEMU_TAP_ORIGIN_BOTTOM_LEFT = 0, // OpenGL convention (glViewport)
        SEMU_TAP_ORIGIN_TOP_LEFT    = 1  // Vulkan / most window-system conventions
    };

    typedef struct {
        int abi;          // = SEMU_TAP_ABI. Compositor ignores mismatched reports.
        int active;       // 1 = live game content this frame; 0 = menu -> pass through.

        int fb_width;     // output framebuffer size in pixels (the presented surface)
        int fb_height;

        int content_x;    // the game content rectangle inside the framebuffer, in
        int content_y;    // pixels, measured from `origin`. The emulator's REAL
        int content_w;    // viewport — exactly where it drew the game this frame.
        int content_h;

        int native_w;     // source/core native resolution (e.g. 320x240). Drives
        int native_h;     // scanline density + integer-scale math in the compositor.

        int rotation;     // content rotation, degrees clockwise: 0 / 90 / 180 / 270.
        int origin;       // one of SEMU_TAP_ORIGIN_*.
        int reserved[4];  // future use; zero-initialize.
    } SemuTapState;

    // Implemented and EXPORTED by the compositor (libsemutap). Emulators must not
    // link it directly — semu_tap_report_safe() resolves it at runtime instead.
    void semu_tap_report(const SemuTapState* state);

    #if !defined(SEMU_TAP_NO_HELPER)
    #include <dlfcn.h>
    static inline void semu_tap_report_safe(const SemuTapState* s) {
        static void (*fn)(const SemuTapState*);
        static int resolved;
        if (!resolved) { resolved = 1; fn = (void (*)(const SemuTapState*))dlsym(RTLD_DEFAULT, "semu_tap_report"); }
        if (fn && s) fn(s);
    }
    #endif

    #ifdef __cplusplus
    }
    #endif
    #endif // SEMU_TAP_H
  '';

  # --- Linux tap build: retroarch-semu -------------------------------------
  linuxTapBuild = (retroarch-bare.overrideAttrs (previous: {
    pname = "retroarch-semu";

    patches = (previous.patches or [ ]) ++ [ ./retroarch.patch ];

    # the patch's `#include "semu_tap.h"` (in gfx/video_driver.c) resolves
    # to gfx/semu_tap.h
    postPatch = (previous.postPatch or "") + ''
      cp ${semuTapHeader} gfx/semu_tap.h
    '';

    # enable the tap report block
    env = (previous.env or { }) // {
      NIX_CFLAGS_COMPILE =
        (previous.env.NIX_CFLAGS_COMPILE or "") + " -DHAVE_SEMU_TAP";
    };

    # the launch contract addresses the tapped binary by its own name
    # (emulator.json: ''${asset_root}/bin/retroarch-semu)
    postInstall = (previous.postInstall or "") + ''
      ln -s $out/bin/retroarch $out/bin/retroarch-semu
    '';
  }));

  # --- macOS build: official universal DMG + buildbot cores ----------------
  macVersion = "1.22.2";
  macArch = if stdenv.hostPlatform.isAarch64 then "arm64" else "x86_64";
  macCoreHashes = {
    arm64 = {
      gambatte = "sha256-i1LZ58cF3blG0vqwK3RkkBc3REU5tTEh5rMaQJonte4=";
      mgba = "sha256-p/YKT5dcZhoXtP4qEkD87/FIBbsaXPjV+lWj8KoIocY=";
      genesis_plus_gx = "sha256-tbkfsiJNh8zN+VTUHmXgF/iAFqP7JGg96XIVMewCcSo=";
      snes9x = "sha256-HF7Q+lblEHy2qGGlrvc62YCL0VF8miAouno/31sziuU=";
      mesen = "sha256-suOxgCRBFdduyHR1rxD2pgy96G2Ye2qf6shlc2Sj1nM=";
      desmume = "sha256-ixjCY8kaFjDOuoi+hGggvYtrjYhz5UDyXULIpgLQcs8=";
      ppsspp = "sha256-2EehMYawCTxEdI5nJIMrlq4ZZLUoKb+oLCDcIpFnVmM=";
      flycast = "sha256-HG22yjl4TSLXm9dHUBJEfzjsqKM7DWQg34QeUXdNsWo=";
      mednafen_psx_hw = "sha256-ev03pEiVlK8Yc2FKbRC88Ge44co/FtkBQM54huqzDng=";
    };
    x86_64 = {
      gambatte = "sha256-9gXMqq+3rai3KeTAfPyuf8upKUH7a/hbfeInK/3mWRM=";
      mgba = "sha256-dGZGqhXLTn2FEsdHJzBeKm2cPYeygVbje3aJ5SyNm5A=";
      genesis_plus_gx = "sha256-3jDrz1lobNmsoieklRpH3a1Y1Fqt8MNSh/GSCK1IjXo=";
      snes9x = "sha256-OkNdZfnh2V/RIvbonXPqd0jeyoYkVvXWaTDutwIc/Hw=";
      mesen = "sha256-2Wf8AZ1pPP/eMSAGITuEDq8z7n7n6PS1ZfZ1xNMONSA=";
      desmume = "sha256-r2L3tLvVPxN312fw7kgCQhPP4XCS0b4M/vGt1mWfCrE=";
      ppsspp = "sha256-Ly9O4Zp0sQtWNU30hDwW/tCVWM0+imbrubBND1KTT00=";
      flycast = "sha256-oMml323FE7KDmw8JpdUJCgo/r4xnpKl9FLJ6q0R6I8M=";
      mednafen_psx_hw = "sha256-f5oG3PiKA7Qct/oa/wtR5+nlmSOURNCllul9lI1i5qs=";
    };
  };
  macCoreZip = core: fetchurl {
    url = "https://buildbot.libretro.com/nightly/apple/osx/${macArch}/latest/${core}_libretro.dylib.zip";
    name = "${core}_libretro.dylib.zip";
    hash = macCoreHashes.${macArch}.${core} or (throw
      "retroarch.nix: no buildbot hash for core '${core}' (${macArch}); prefetch and add it");
  };

  macBuild = stdenv.mkDerivation {
    pname = "retroarch";
    version = macVersion;
    src = fetchurl {
      url = "https://buildbot.libretro.com/stable/${macVersion}/apple/osx/universal/RetroArch_Metal.dmg";
      hash = "sha256-gbeRIbom1TkGSuE7TQQZoSDD0WWvvmVs9fVBKxX9tDQ=";
    };
    sourceRoot = ".";
    nativeBuildInputs = [ undmg unzip ];
    installPhase = ''
      mkdir -p $out/Applications $out/cores $out/bin
      cp -r RetroArch.app $out/Applications/

      ${lib.concatMapStringsSep "\n"
        (core: "unzip -o ${macCoreZip core} -d $out/cores/") cores}

      # --appendconfig points RetroArch at the bundled cores while the user
      # config stays the writable one under ~/Library.
      echo "libretro_directory = \"$out/cores\"" > $out/cores/path.cfg
      cat > $out/bin/retroarch <<'WRAPPER'
      #!/bin/bash
      SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
      CONFIG="$HOME/Library/Application Support/RetroArch/retroarch.cfg"
      exec "$SCRIPT_DIR/Applications/RetroArch.app/Contents/MacOS/RetroArch" \
        --config="$CONFIG" \
        --appendconfig="$SCRIPT_DIR/cores/path.cfg" \
        "$@"
      WRAPPER
      chmod +x $out/bin/retroarch
    '';
    meta = {
      description = "RetroArch with the ${toString (lib.length cores)} cores the system contracts route on macOS";
      homepage = "https://www.retroarch.com";
      platforms = lib.platforms.darwin;
      license = lib.licenses.gpl3;
    };
  };
in
if stdenv.hostPlatform.isDarwin then macBuild else linuxTapBuild
