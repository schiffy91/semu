# pcsx2.nix — the PCSX2 package recipes, owned next to its contract
# (emulator.json declares the macos native backend this satisfies; the linux
# contract routes PCSX2 through flatpak, never nix).
#
# TARGET STATE (owner directive): build from source so Semu patches — the
# tap's gameplay-vs-menu active flag (rendering/tap/semu_tap.h) — can compile
# in. The pinned-source expression below is exposed as passthru.sourceBuild
# for iteration (`nix build .#pcsx2.sourceBuild`); dependency wiring follows
# the nixpkgs pcsx2 2.6.3 recipe (linux-only upstream) trimmed to darwin.
#
# BLOCKER (2026-07-02, honest attempt on aarch64-darwin — 4 iterations, the
# last reaching 86% with ALL C++ compiled): the GS Metal renderer
# precompiles its shaders with `xcrun metal` / `xcrun metallib`
# (pcsx2/CMakeLists.txt generateMetallib). Apple ships that toolchain only
# inside Xcode — not in CommandLineTools and not in nixpkgs — so the pure
# nix sandbox has no `metal` tool and the bundle cannot be produced
# ("error: tool 'metal' not found"). Until a nix-usable Metal shader
# compiler exists (or upstream gains runtime MSL compilation like
# RetroArch), the ACTIVE variant stays the upstream release bundle. The
# postPatch fast_float fixes below are proven against the pin and keep the
# expression one blocker away from green.
{ lib
, stdenv
, fetchFromGitHub
, fetchurl
, cmake
, pkg-config
, qt6
, sdl3
, curl
, ffmpeg
, freetype
, libpcap
, libpng
, libjpeg
, libwebp
, lz4
, shaderc
, soundtouch
, vulkan-headers
, moltenvk
, zip
, zstd
, plutovg
, plutosvg
, kddockwidgets
}:

let
  version = "2.6.3";

  # ONE pinned source: tag v2.6.3, resolved 2026-07-02 via git ls-remote
  # (pin the commit, not the movable tag). PCSX2 vendors 3rdparty/ in-tree,
  # no submodules needed.
  source = fetchFromGitHub {
    owner = "PCSX2";
    repo = "pcsx2";
    rev = "bc8151d2a46d4aba039ea5580afbfc7bfcf6d730";
    hash = "sha256-85PZ7ZDoannmwoFeKM7hm7fQS1X2MPxAwm6k+Sa+bGc=";
  };

  # Semu patches hook in here once the source build is the active variant.
  patches = [ ];

  sourceBuild = stdenv.mkDerivation {
    pname = "pcsx2";
    inherit version;
    src = source;
    inherit patches;

    postPatch = ''
      substituteInPlace cmake/Pcsx2Utils.cmake \
        --replace-fail 'set(PCSX2_GIT_TAG "")' 'set(PCSX2_GIT_TAG "v${version}")'

      # PCSX2 defines _M_ARM64 globally on aarch64, which flips the vendored
      # fast_float into its MSVC paths (#include <intrin.h>, __umulh) — dead
      # on macOS.
      substituteInPlace 3rdparty/fast_float/include/fast_float/float_common.h \
        --replace-fail '(defined(_M_ARM64) && !defined(__MINGW32__))' \
          '(defined(_M_ARM64) && !defined(__MINGW32__) && !defined(__APPLE__))' \
        --replace-fail '#if defined(_M_ARM64) && !defined(__MINGW32__)' \
          '#if defined(_M_ARM64) && !defined(__MINGW32__) && !defined(__APPLE__)'
    '';

    nativeBuildInputs = [ cmake pkg-config zip qt6.wrapQtAppsHook ];
    buildInputs = [
      curl
      ffmpeg
      freetype
      kddockwidgets
      libjpeg
      libpcap
      libpng
      libwebp
      lz4
      moltenvk
      plutosvg
      plutovg
      qt6.qtbase
      qt6.qtsvg
      qt6.qttools
      sdl3
      shaderc
      soundtouch
      vulkan-headers
      zstd
    ];

    cmakeFlags = [
      (lib.cmakeBool "USE_LINKED_FFMPEG" true)
      # DISABLE_ADVANCE_SIMD is an x86-only distribution knob; passing it on
      # arm64 wrongly spawns the GS multi-ISA (sse4/avx/avx2) targets.
    ] ++ lib.optionals stdenv.hostPlatform.isx86_64 [
      (lib.cmakeBool "DISABLE_ADVANCE_SIMD" true)
    ];

    # the launch contract (emulator.json macos.executable) addresses
    # Applications/PCSX2.app/Contents/MacOS/PCSX2
    installPhase = ''
      runHook preInstall
      mkdir -p $out/Applications
      cp -r bin/PCSX2.app $out/Applications/PCSX2.app
      runHook postInstall
    '';

    meta = {
      description = "PlayStation 2 emulator, built from source";
      homepage = "https://pcsx2.net";
      platforms = lib.platforms.darwin;
      license = lib.licenses.gpl3;
    };
  };

  # --- ACTIVE variant: the upstream release bundle --------------------------
  prebuilt = stdenv.mkDerivation {
    pname = "pcsx2";
    inherit version;
    src = fetchurl {
      url = "https://github.com/PCSX2/pcsx2/releases/download/v${version}/pcsx2-v${version}-macos-Qt.tar.xz";
      hash = "sha256-y3ueYzDxq/DPkslAZffrmD0PqK/8/msMy5wqTr8Gfxo=";
    };
    sourceRoot = ".";
    installPhase = ''
      mkdir -p $out/Applications
      cp -r PCSX2-v${version}.app $out/Applications/PCSX2.app
    '';
    passthru = { inherit sourceBuild; };
    meta = {
      description = "PlayStation 2 emulator";
      homepage = "https://pcsx2.net";
      platforms = lib.platforms.darwin;
      license = lib.licenses.gpl3;
    };
  };
in
if stdenv.hostPlatform.isDarwin then prebuilt
else throw "pcsx2.nix: unsupported platform ${stdenv.hostPlatform.system} (the linux contract routes PCSX2 through flatpak, never nix)"
