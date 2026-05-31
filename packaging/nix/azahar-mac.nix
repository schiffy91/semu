# Azahar for macOS — pre-built from official GitHub release.
# nixpkgs version (2124.3) crashes on macOS, so we use the latest release.
{ lib, stdenv, fetchurl, unzip }:

let
  version = "2125.0.1";
in
stdenv.mkDerivation {
  pname = "azahar";
  inherit version;
  src = fetchurl {
    url = "https://github.com/azahar-emu/azahar/releases/download/${version}/azahar-macos-universal-${version}.zip";
    hash = "sha256-b7lEWXztuGW+XdCsbSF7TnE3wnAGZoJY2+OoMpeuJ68=";
  };
  sourceRoot = "azahar-macos-universal-${version}";
  nativeBuildInputs = [ unzip ];
  installPhase = ''
    mkdir -p $out/Applications $out/bin
    cp -r Azahar.app $out/Applications/azahar.app
    ln -s $out/Applications/azahar.app/Contents/MacOS/azahar $out/bin/azahar
  '';
  meta = {
    description = "Nintendo 3DS emulator";
    homepage = "https://azahar-emu.org";
    platforms = [ "aarch64-darwin" "x86_64-darwin" ];
    license = lib.licenses.gpl2Plus;
  };
}
