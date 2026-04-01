{ lib, stdenv, fetchurl, appimageTools ? null, undmg ? null }:

let
  version = "3.4.0";
  sources = {
    aarch64-darwin = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/243196872/download";
      hash = "sha256-fpwMwi7vwbd0n/+Vy1JBDWGJtZ7GkUSBd9b6wbErQ6w=";
      name = "ES-DE_${version}-arm64.dmg";
    };
    x86_64-linux = {
      url = "https://gitlab.com/es-de/emulationstation-de/-/package_files/246875981/download";
      hash = "sha256-TLZs/JIwmXEfoP7Rnuhrl0SmKU4C4//Rnuhn93qI7H4=";
      name = "ES-DE_x64.AppImage";
    };
  };
in
if stdenv.hostPlatform.isDarwin then
  stdenv.mkDerivation {
    pname = "es-de";
    inherit version;
    src = fetchurl sources.aarch64-darwin;
    sourceRoot = ".";
    nativeBuildInputs = [ undmg ];
    installPhase = ''
      mkdir -p $out/Applications
      cp -r "ES-DE.app" $out/Applications/
    '';
    meta = {
      description = "EmulationStation Desktop Edition — emulator frontend";
      homepage = "https://es-de.org";
      platforms = [ "aarch64-darwin" ];
      license = lib.licenses.mit;
    };
  }
else
  appimageTools.wrapType2 {
    pname = "es-de";
    inherit version;
    src = fetchurl sources.x86_64-linux;
    meta = {
      description = "EmulationStation Desktop Edition — emulator frontend";
      homepage = "https://es-de.org";
      platforms = [ "x86_64-linux" ];
      license = lib.licenses.mit;
    };
  }
