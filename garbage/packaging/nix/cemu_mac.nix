{ lib, stdenv, fetchurl, undmg }:

let
  version = "2.6";
in
stdenv.mkDerivation {
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
  meta = {
    description = "Wii U emulator";
    homepage = "https://cemu.info";
    platforms = [ "x86_64-darwin" "aarch64-darwin" ];
    license = lib.licenses.mpl20;
  };
}
