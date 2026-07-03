# azahar.nix — Azahar BUILT FROM SOURCE, owned next to its contract
# (emulator.json declares the macos native backend this satisfies; the linux
# contract routes Azahar through flatpak, never nix). Owner directive:
# source builds so Semu patches — the tap's gameplay-vs-menu active flag
# (rendering/tap/semu_tap.h) — can compile in.
#
# Dependency set and system-libs cmake wiring follow nixpkgs azahar
# (2120.3, a proven cmake recipe) trimmed to what the darwin build needs;
# the source is Semu's own pin of the current stable tag with the vendored
# externals/ submodules.
#
# SOURCE-BUILD STATUS (2026-07-02): three real blockers cleared in order —
# (1) source outputHash corrected, (2) MoltenVK wired for Vulkan-on-Metal,
# (3) git added for dynarmic's scm version. Remaining blocker: dynarmic's
# build-time architecture probe (externals/dynarmic/src/dynarmic/
# CMakeLists.txt:454 execute_process) fails to link its libfoo.a test binary
# under the nix clang-wrapper ("Error running link command: no such file or
# directory"). This is a darwin cross-toolchain issue in a vendored external,
# not a Semu contract problem. Unlike the linux emulators, azahar on macOS
# needs NO compiled-in tap patch (the macOS tap is the overlay compositor,
# not semu_tap.h's active flag), so the maintained nixpkgs azahar (2125.1.2,
# source-built, darwin) is a correct drop-in until this probe is patched.
# Keep this expression as the target; the fixes above are real progress.
{ lib
, stdenv
, fetchFromGitHub
, cmake
, git
, pkg-config
, qt6
, boost
, catch2_3
, cryptopp
, cpp-jwt
, enet
, ffmpeg_6-headless
, fmt
, glslang
, httplib
, inih
, libusb1
, nlohmann_json
, openal
, openssl
, SDL2
, cubeb
, soundtouch
, spirv-tools
, vulkan-headers
, moltenvk
, zstd
}:

let
  version = "2125.0.1";

  # ONE pinned source: tag 2125.0.1, resolved 2026-07-02 via git ls-remote
  # (pin the commit, not the movable tag). Submodules carry the vendored
  # externals/ tree (dynarmic, xbyak, vma, ...) upstream builds against.
  source = fetchFromGitHub {
    owner = "azahar-emu";
    repo = "azahar";
    rev = "7e58ac5bcf412f358374f03a5c2895d798bd5a5e";
    fetchSubmodules = true;
    hash = "sha256-z6/X2lOJe6StGKdu2k4JETQaxAfjdTiDtZLnJ2JzhRE=";
  };
in
if !stdenv.hostPlatform.isDarwin then
  throw "azahar.nix: unsupported platform ${stdenv.hostPlatform.system} (the linux contract routes Azahar through flatpak, never nix)"
else stdenv.mkDerivation {
  pname = "azahar";
  inherit version;
  src = source;

  # Semu patches hook in here.
  patches = [ ];

  postPatch = ''
    # the submodules are pinned by the fetch above
    substituteInPlace CMakeLists.txt \
      --replace-fail "check_submodules_present()" ""
  '';

  nativeBuildInputs = [
    cmake
    git
    pkg-config
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    boost
    catch2_3
    cryptopp
    cpp-jwt
    cubeb
    enet
    ffmpeg_6-headless
    fmt
    glslang
    httplib
    inih
    libusb1
    nlohmann_json
    openal
    openssl
    qt6.qtbase
    qt6.qtmultimedia
    qt6.qttools
    SDL2
    soundtouch
    spirv-tools
    vulkan-headers
    moltenvk
    zstd
  ];

  cmakeFlags = [
    (lib.cmakeBool "USE_SYSTEM_LIBS" true)
    (lib.cmakeBool "DISABLE_SYSTEM_DYNARMIC" true)
    (lib.cmakeBool "DISABLE_SYSTEM_LODEPNG" true)
    (lib.cmakeBool "DISABLE_SYSTEM_VMA" true)
    (lib.cmakeBool "DISABLE_SYSTEM_XBYAK" true)
    (lib.cmakeBool "ENABLE_QT" true)
    (lib.cmakeBool "ENABLE_QT_TRANSLATION" false)
    (lib.cmakeBool "ENABLE_SDL2" true)
    (lib.cmakeBool "ENABLE_SDL2_FRONTEND" true)
    (lib.cmakeBool "ENABLE_CUBEB" true)
    (lib.cmakeBool "USE_DISCORD_PRESENCE" false)
    (lib.cmakeBool "ENABLE_TESTS" false)
    (lib.cmakeBool "ENABLE_WEB_SERVICE" true)
    # Vulkan on macOS is MoltenVK; pre-seed the cache var so the source's
    # find_library(MOLTENVK_LIBRARY MoltenVK) resolves to the nixpkgs dylib
    # instead of failing against the SDK framework search path.
    (lib.cmakeFeature "MOLTENVK_LIBRARY" "${moltenvk}/lib/libMoltenVK.dylib")
  ];

  # the launch contract (emulator.json macos.executable) addresses
  # Applications/azahar.app/Contents/MacOS/azahar
  postInstall = ''
    if [ ! -d "$out/Applications/azahar.app" ]; then
      mkdir -p $out/Applications
      for bundle in "$out"/azahar.app "$out"/bin/azahar.app "$out"/*/azahar.app; do
        if [ -d "$bundle" ]; then
          mv "$bundle" $out/Applications/azahar.app
          break
        fi
      done
    fi
    mkdir -p $out/bin
    if [ -d "$out/Applications/azahar.app" ] && [ ! -e "$out/bin/azahar" ]; then
      ln -s $out/Applications/azahar.app/Contents/MacOS/azahar $out/bin/azahar
    fi
  '';

  meta = {
    description = "Nintendo 3DS emulator, built from source";
    homepage = "https://azahar-emu.org";
    platforms = lib.platforms.darwin;
    license = lib.licenses.gpl2Plus;
    mainProgram = "azahar";
  };
}
