{ lib, stdenv, fetchurl }:

let
  version = "2.6.3";
in
stdenv.mkDerivation {
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
  meta = {
    description = "PlayStation 2 emulator";
    homepage = "https://pcsx2.net";
    platforms = [ "x86_64-darwin" "aarch64-darwin" ];
    license = lib.licenses.gpl3;
  };
}
