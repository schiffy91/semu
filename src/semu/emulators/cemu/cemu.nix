# cemu.nix — the Cemu package recipes, owned next to its contract
# (emulator.json declares the macos native backend this satisfies).
#
# TARGET STATE (owner directive): build from source so Semu patches — the
# tap's gameplay-vs-menu active flag (rendering/tap/semu_tap.h) — can compile
# in. The pinned-source expression below is exposed as passthru.sourceBuild
# for iteration (`nix build .#cemu.sourceBuild`).
#
# BLOCKER (2026-07-02, honest attempt on this aarch64-darwin machine):
# `nix build .#cemu.sourceBuild` dies before reaching Cemu code — the
# required fmt_9 (fmt 9.1.0, Cemu pins `find_package(fmt 9 REQUIRED)`)
# FTBFS on aarch64-darwin with the pin's clang 21 / libc++
# ("'is_floating_point' cannot be specialized", format-impl-test.cc:381).
# Behind that sits the structural blocker: Cemu's PowerPC recompiler is
# x86_64-only on macOS — upstream ships only x64 bundles
# (cemu-2.6-macos-12-x64.dmg, Rosetta on Apple silicon) and its CI
# cross-builds with CMAKE_OSX_ARCHITECTURES=x86_64 against vcpkg deps, so a
# faithful nix build needs the whole dependency closure as x86_64-darwin
# under Rosetta — a toolchain story this flake does not carry. Until
# upstream publishes an arm64 macOS story (or the nixpkgs pin moves past
# the fmt_9 FTBFS for a Rosetta attempt), the ACTIVE variant stays the
# upstream x64 DMG running under Rosetta.
{ lib
, stdenv
, fetchFromGitHub
, fetchurl
, undmg
, cmake
, ninja
, nasm
, pkg-config
, boost
, cubeb
, curl
, fmt_9
, glm
, glslang
, hidapi
, libpng
, libusb1
, libzip
, pugixml
, rapidjson
, SDL2
, vulkan-headers
, wxGTK32
, zarchive
, zstd
, openssl
}:

let
  version = "2.6";

  # ONE pinned source: tag v2.6, resolved 2026-07-02 via git ls-remote (pin
  # the commit, not the movable tag). Submodules carry the vendored
  # dependencies/ tree (imgui, ih264d, ...) upstream builds against.
  source = fetchFromGitHub {
    owner = "cemu-project";
    repo = "Cemu";
    rev = "a6fb0a48eb437a8a41c13b782ac8ae0433bf8f98";
    fetchSubmodules = true;
    hash = "sha256-d8tnq+Cyjnh3sEn75MLL8Q7agzT1tDZEsDANIe0KK5s=";
  };

  # Semu patches hook in here once the source build is the active variant —
  # none are needed today, and none would buy bezels: Cemu presents through
  # Vulkan, which the LD_PRELOAD GL interposition cannot see, so linux bezels
  # ride the vkBasalt assets (assets/vkbasalt) until a Vulkan layer exists;
  # macOS presents via the external overlay.
  patches = [ ];

  sourceBuild = stdenv.mkDerivation {
    pname = "cemu";
    inherit version;
    src = source;
    inherit patches;

    nativeBuildInputs = [ cmake ninja nasm pkg-config ];
    buildInputs = [
      boost
      cubeb
      curl
      fmt_9
      glm
      glslang
      hidapi
      libpng
      libusb1
      libzip
      openssl
      pugixml
      rapidjson
      SDL2
      vulkan-headers
      wxGTK32
      zarchive
      zstd
    ];

    cmakeFlags = [
      (lib.cmakeBool "ENABLE_VCPKG" false)
      (lib.cmakeBool "MACOS_BUNDLE" true)
      (lib.cmakeBool "PORTABLE" false)
      (lib.cmakeFeature "EMULATOR_VERSION_MAJOR" "2")
      (lib.cmakeFeature "EMULATOR_VERSION_MINOR" "6")
    ];

    installPhase = ''
      runHook preInstall
      mkdir -p $out/Applications
      cp -r ../bin/Cemu.app $out/Applications/
      runHook postInstall
    '';

    meta = {
      description = "Wii U emulator, built from source";
      homepage = "https://cemu.info";
      platforms = lib.platforms.darwin;
      license = lib.licenses.mpl20;
    };
  };

  # --- ACTIVE variant: upstream x64 DMG (Rosetta on Apple silicon) ---------
  prebuilt = stdenv.mkDerivation {
    pname = "cemu";
    inherit version;
    src = fetchurl {
      url = "https://github.com/cemu-project/Cemu/releases/download/v${version}/cemu-${version}-macos-12-x64.dmg";
      hash = "sha256-aYxLKY+UmD5NbDDpaHuo/wUJTdOTCDfFEEzdwLCknk4=";
    };
    sourceRoot = ".";
    nativeBuildInputs = [ undmg ];
    installPhase = ''
      mkdir -p $out/Applications
      cp -r Cemu.app $out/Applications/
    '';
    passthru = { inherit sourceBuild; };
    meta = {
      description = "Wii U emulator";
      homepage = "https://cemu.info";
      platforms = lib.platforms.darwin;
      license = lib.licenses.mpl20;
    };
  };
in
if stdenv.hostPlatform.isDarwin then prebuilt
else throw "cemu.nix: unsupported platform ${stdenv.hostPlatform.system} (the linux contract routes Cemu through flatpak, never nix)"
