# cemu.nix — the Cemu package recipe, owned next to its contract
# (emulator.json declares the macos native backend this satisfies).
# x64 build; runs under Rosetta on Apple silicon.
{ lib, stdenv, fetchurl, undmg }:

stdenv.mkDerivation rec {
  pname = "cemu";
  version = "2.6";
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
    platforms = lib.platforms.darwin;
    license = lib.licenses.mpl20;
  };
}
