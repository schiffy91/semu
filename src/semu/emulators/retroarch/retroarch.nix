# retroarch.nix — the RetroArch package recipes, owned next to the emulator
# contract. Both platform variants build the SAME pinned upstream source
# (owner directive: source builds so Semu patches — the tap's
# gameplay-vs-menu active flag — compile in); packaging/nix only discovers
# the inventory from the JSON contracts and injects the contract-derived
# `cores` list.
#
# macOS (the emulator.json macos nix backend): built from the pinned source
# with the Metal driver plus vendored glslang/SPIRV-Cross (nixpkgs marks its
# retroarch broken on Darwin; this recipe compiles it directly), the same
# tap report compiled in (dormant until a macOS compositor exports
# semu_tap_report), and the sibling retroarch_darwin.patch fixing the
# upstream `make`-on-macOS breakages the Xcode build masks. The libretro
# CORES stay buildbot binary artifacts for now — libretro has no unified
# per-core source build story, and the zips come from the buildbot's
# rolling "latest" nightly directory, so those hashes go stale when the
# buildbot rotates; re-prefetch on mismatch (2026-07-01 pins).
#
# Linux: the Semu tap build of the same pinned source. The sibling
# retroarch.patch makes video_driver_frame report the real content viewport
# + active state to the Semu compositor every frame (semu_tap_report_safe
# resolves at runtime, so an uncomposited run is a no-op). Build wiring
# (deps/configure flags) reuses nixpkgs retroarch-bare, following the proven
# retroarch-semu recipe (git history: packaging/nix/retroarch-semu.nix at
# 81dd8cc — patch + tap header + -DHAVE_SEMU_TAP). The Deck's runtime
# flatpak RetroArch remains a runtime concern, not packaged here.
{ lib, stdenv, fetchFromGitHub, fetchurl, unzip, pkg-config, writeText, zlib
, retroarch-bare, cores ? [ ] }:

let
  version = "1.22.2";

  # ONE pinned source for every platform variant: tag v1.22.2, resolved
  # 2026-07-02 via git ls-remote (pin the commit, not the movable tag).
  source = fetchFromGitHub {
    owner = "libretro";
    repo = "RetroArch";
    rev = "69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576";
    hash = "sha256-+3jgoh6OVbPzW5/nCvpB1CRgkMTBxLkYMm6UV16/cfU=";
  };

  # Patches applied on every platform. The Linux tap build appends the
  # sibling retroarch.patch below; the macOS build appends the sibling
  # retroarch_darwin.patch (the five upstream `make`-on-macOS breakages the
  # Xcode build masks — see that file's header). Both verified to apply to
  # the v1.22.2 pin.
  sharedPatches = [ ];
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
    inherit version;
    src = source;

    patches = (previous.patches or [ ]) ++ sharedPatches
      ++ [ ./retroarch.patch ];

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

  # --- macOS build: the pinned source with Metal + buildbot cores ----------
  macArch = if stdenv.hostPlatform.isAarch64 then "arm64" else "x86_64";
  macCoreHashes = {
    arm64 = {
      gambatte = "sha256-PgWveksmEl8m23RfGe0TUASD1sF8NCU2ahD7rQxLcOc=";
      mgba = "sha256-V7wXwP8NCw5OSo7V9uMbT4oNcVGMnxAuGgzmfdvAq7Q=";
      genesis_plus_gx = "sha256-x1LzTLrQpBO2xGOCyt4QwDynhCS1YK7JAGZ/3PTat44=";
      snes9x = "sha256-1+hLHACkd7b8ymtGEgbzkbnNTxLaa+wtvnhldAHFuS0=";
      mesen = "sha256-y5qjFuwrh8UHrK1KS5hp57GXVg7o1sEzM9oOw34AyBw=";
      desmume = "sha256-TdvYhw6sPquqR69wV+y4Im5kyKI70JuoRL2p1acoc94=";
      ppsspp = "sha256-ozmQ8SXcDj5hQys30C2iVneyEzKXQuy6m7c3leh+ygM=";
      flycast = "sha256-TuTvmuGizfJOWZsHQlxz94MEPwyuJRYFRYbA0AzR6dk=";
      mednafen_psx_hw = "sha256-kYNxIhDc4bX5dQGqPWQGrZnbkjSWekMU4Osx1+pCF40=";
      mupen64plus_next = "sha256-NxGrZz9yKm7xZZi7CSSLp8KlXKTGaYYZ4865iqH6eyQ=";
      azahar = "sha256-3tazyoi57gQjUiwnzlxtSksWcaMZEiqbU4fd1kfJvoM=";
    };
    x86_64 = {
      gambatte = "sha256-xmUVaSVKjCipHBdlfPanQvT5LsBMh/RwDN5Kf4u6vRU=";
      mgba = "sha256-G2IVABRf4l05IDiLXD6WzPUAZkuiElCG/ZlF6hmzTKk=";
      genesis_plus_gx = "sha256-FgUkWQlbBYD4azOk+dpLIsZuTGm/QL4A+BTYlQJHqx4=";
      snes9x = "sha256-PeDlLkHfCFx0yNrd8mwds1N5q0QZnDcAZwz7LGBhjs0=";
      mesen = "sha256-gxC64F+vQxKMBSxV40XTSVhQUPWcTVT81AHvOIFlfOs=";
      desmume = "sha256-3m6ffxS8jVmlpEE/Wzc1zkhRE82pZHTCqIjghpZga1Y=";
      ppsspp = "sha256-UC9bTtDXEE24SI3XBMuvbGcAZNF/SDkxlDPyt7Hyxg4=";
      flycast = "sha256-gkfOWwTl5B8Ht1g2V11GBLp3JqjECm8OFzGbZ3Cdahk=";
      mednafen_psx_hw = "sha256-N0CsujMVWaFWMLRwtT0TdEoO+e/NgIUHkZVCq1i8rIs=";
      mupen64plus_next = "sha256-xK5HW0Oox1giR23ePSBF7Pj4bVPQJWw0UOOmIWpH3A4=";
      azahar = "sha256-sdHtFNVpfO9ri3NcFkx720IRcD8uX6V0gyQV2L0saBM=";
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
    inherit version;
    src = source;
    # same tap report as the Linux build (a no-op until a macOS compositor
    # loads and exports semu_tap_report) plus the make-on-macOS fixes
    patches = sharedPatches ++ [ ./retroarch.patch ./retroarch_darwin.patch ];

    postPatch = ''
      cp ${semuTapHeader} gfx/semu_tap.h
    '';

    env.NIX_CFLAGS_COMPILE = "-DHAVE_SEMU_TAP";

    nativeBuildInputs = [ pkg-config unzip ];
    buildInputs = [ zlib ];

    # qb configure (not autoconf): Metal + the vendored (builtin) glslang /
    # SPIRV-Cross slang pipeline the Metal driver requires. nixpkgs zlib
    # replaces the vendored deps/libz (whose classic-MacOS fdopen macro
    # breaks against modern SDK headers). The self-update paths are disabled
    # so RetroArch never writes into the store; Qt is the desktop companion
    # UI Semu never launches; ffmpeg recording is off to keep the closure at
    # the SDK frameworks.
    configureFlags = [
      "--enable-metal"
      "--enable-builtinglslang"
      "--disable-builtinzlib"
      "--disable-qt"
      "--disable-ffmpeg"
      # the make build never compiles the CoreAudio microphone driver the
      # Xcode project carries; Semu has no microphone use anyway
      "--disable-microphone"
      "--disable-update_cores"
      "--disable-update_assets"
      "--disable-update_core_info"
    ];

    enableParallelBuilding = true;

    # `make install` puts the real binary at bin/retroarch; Semu's launch
    # contract wraps it so the bundled cores resolve while the user config
    # stays the writable one under ~/Library.
    postInstall = ''
      mkdir -p $out/libexec $out/cores
      mv $out/bin/retroarch $out/libexec/retroarch

      ${lib.concatMapStringsSep "\n"
        (core: "unzip -o ${macCoreZip core} -d $out/cores/") cores}

      echo "libretro_directory = \"$out/cores\"" > $out/cores/path.cfg
      cat > $out/bin/retroarch <<'WRAPPER'
      #!/bin/bash
      SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
      CONFIG="$HOME/Library/Application Support/RetroArch/retroarch.cfg"
      exec "$SCRIPT_DIR/libexec/retroarch" \
        --config="$CONFIG" \
        --appendconfig="$SCRIPT_DIR/cores/path.cfg" \
        "$@"
      WRAPPER
      chmod +x $out/bin/retroarch
    '';

    meta = {
      description = "RetroArch (Metal, built from source) with the ${toString (lib.length cores)} cores the system contracts route on macOS";
      homepage = "https://www.retroarch.com";
      platforms = lib.platforms.darwin;
      license = lib.licenses.gpl3;
      mainProgram = "retroarch";
    };
  };
in
if stdenv.hostPlatform.isDarwin then macBuild
else if stdenv.hostPlatform.isLinux then linuxTapBuild
else throw "retroarch.nix: unsupported platform ${stdenv.hostPlatform.system} (darwin and linux only)"
