# pcsx2.nix — the PCSX2 package recipe, owned next to its contract
# (emulator.json declares the macos native backend this satisfies). The
# official Qt release tarball; nixpkgs pcsx2 does not build on Darwin.
{ lib, stdenv, fetchurl }:

stdenv.mkDerivation rec {
  pname = "pcsx2";
  version = "2.6.3";
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
    platforms = lib.platforms.darwin;
    license = lib.licenses.gpl3;
  };
}
