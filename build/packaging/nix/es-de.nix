{ lib
, stdenv
, fetchurl
, fetchFromGitLab
, appimageTools ? null
, undmg ? null
, cmake ? null
, pkg-config ? null
, gettext ? null
, curl ? null
, ffmpeg ? null
, freeimage ? null
, freetype ? null
, harfbuzz ? null
, icu ? null
, libgit2 ? null
, pugixml ? null
, SDL2 ? null
, alsa-lib ? null
, bluez ? null
, libGL ? null
, libGLU ? null
, poppler ? null
, steamDeck ? false
}:

let
  version = "3.4.0";
  sources = {
    aarch64-darwin = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/243196872/download";
      hash = "sha256-fpwMwi7vwbd0n/+Vy1JBDWGJtZ7GkUSBd9b6wbErQ6w=";
      name = "ES-DE_${version}-arm64.dmg";
    };
    x86_64-darwin = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/243196947/download";
      hash = "sha256-iDBvw/caxHCIlQ3QuoOcGUYjtxl3IqmjikYBUN4ZH2w=";
      name = "ES-DE_${version}-x64.dmg";
    };
    x86_64-linux = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/246875981/download";
      hash = "sha256-TLZs/JIwmXEc+g7d2D22R0SmKU4C4//Rnuhn93qI7H4=";
      name = "ES-DE_x64.AppImage";
    };
    x86_64-linux-steamdeck = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/246876039/download";
      hash = "sha256-/RaIA4KO3QaDPlxboj36Nmujvs58BUMpVU+8Qj5+lws=";
      name = "ES-DE_x64_SteamDeck.AppImage";
    };
  };

  sourceKey =
    if steamDeck && stdenv.hostPlatform.system == "x86_64-linux"
    then "x86_64-linux-steamdeck"
    else stdenv.hostPlatform.system;

  linuxSource = fetchFromGitLab {
    owner = "es-de";
    repo = "emulationstation-de";
    rev = "v${version}";
    hash = "sha256-poegMKtPtUbdUbAwVj6O+rh7bxou+Wc+IDS3TBHh2LU=";
  };
in
if stdenv.hostPlatform.isDarwin then
  stdenv.mkDerivation {
    pname = "es-de";
    inherit version;
    src = fetchurl (sources.${stdenv.hostPlatform.system} or (throw "ES-DE: unsupported platform ${stdenv.hostPlatform.system}"));
    sourceRoot = ".";
    nativeBuildInputs = [ undmg ];
    installPhase = ''
      mkdir -p $out/Applications
      cp -r "ES-DE.app" $out/Applications/
    '';
    meta = {
      description = "EmulationStation Desktop Edition — emulator frontend";
      homepage = "https://es-de.org";
      platforms = [ "aarch64-darwin" "x86_64-darwin" ];
      license = lib.licenses.mit;
    };
  }
else if stdenv.hostPlatform.system == "x86_64-linux" then
  stdenv.mkDerivation {
    pname = if steamDeck then "es-de-steamdeck" else "es-de";
    inherit version;
    src = linuxSource;
    patches = [ ./patches/es-de/001-semu-settings-utility.patch ];
    nativeBuildInputs = [ cmake pkg-config gettext ];
    buildInputs = [
      curl
      ffmpeg
      freeimage
      freetype
      harfbuzz
      icu
      libgit2
      pugixml
      SDL2
      alsa-lib
      bluez
      libGL
      libGLU
      poppler
    ];
    cmakeFlags = [
      (lib.cmakeBool "APPLICATION_UPDATER" false)
    ];
    postPatch = ''
      grep -F 'SEMU SETTINGS' es-app/src/guis/GuiMenu.cpp >/dev/null
      grep -F 'SEMU_ESDE_SETTINGS_COMMAND' es-app/src/guis/GuiMenu.cpp >/dev/null
    '';
    meta = {
      description = "EmulationStation Desktop Edition — emulator frontend"
        + lib.optionalString steamDeck " (Steam Deck optimized)";
      homepage = "https://es-de.org";
      platforms = [ "x86_64-linux" ];
      license = lib.licenses.mit;
    };
  }
else
  throw "ES-DE: unsupported platform ${stdenv.hostPlatform.system}"
