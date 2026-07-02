# es_de.nix — the ES-DE frontend from official upstream artifacts: DMG on
# macOS, AppImage on x86_64 Linux. steamDeck picks the Steam Deck AppImage,
# tuned for the gamescope/Game Mode session (the generic Linux build loads
# but its window is not presented by gamescope).
{ lib, stdenv, fetchurl, appimageTools ? null, undmg ? null, steamDeck ? false }:

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

  system = stdenv.hostPlatform.system;
  unsupported = throw "ES-DE: unsupported platform ${system}";
in
if stdenv.hostPlatform.isDarwin then
  stdenv.mkDerivation {
    pname = "es-de";
    inherit version;
    src = fetchurl (sources.${system} or unsupported);
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
else if system == "x86_64-linux" then
  appimageTools.wrapType2 {
    pname = if steamDeck then "es-de-steamdeck" else "es-de";
    inherit version;
    src = fetchurl sources.${if steamDeck then "x86_64-linux-steamdeck" else system};
    meta = {
      description = "EmulationStation Desktop Edition — emulator frontend"
        + lib.optionalString steamDeck " (Steam Deck optimized)";
      homepage = "https://es-de.org";
      platforms = [ "x86_64-linux" ];
      license = lib.licenses.mit;
    };
  }
else
  unsupported
