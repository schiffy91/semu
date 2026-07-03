# azahar.nix — the Azahar package recipe, owned next to its contract
# (emulator.json declares the macos native backend this satisfies).
# nixpkgs azahar (2124.3) crashes on macOS; the official release works.
{ lib, stdenv, fetchurl, unzip }:

stdenv.mkDerivation rec {
  pname = "azahar";
  version = "2125.0.1";
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
    platforms = lib.platforms.darwin;
    license = lib.licenses.gpl2Plus;
  };
}
