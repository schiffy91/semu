# dolphin.nix — the Dolphin package recipe, owned next to its contract
# (emulator.json declares the macos native backend this satisfies). The
# official universal DMG; nixpkgs dolphin-emu lags and its Darwin build is
# not the upstream-blessed binary.
{ lib, stdenv, fetchurl, undmg }:

stdenv.mkDerivation rec {
  pname = "dolphin-emu";
  version = "2506a";
  src = fetchurl {
    url = "https://dl.dolphin-emu.org/releases/${version}/dolphin-${version}-universal.dmg";
    hash = "sha256-DqV+rNgKtRy/F6DNYwm1lzFVzEbA+pD16Mb7UO6WZ8w=";
  };
  sourceRoot = ".";
  nativeBuildInputs = [ undmg ];
  installPhase = ''
    mkdir -p $out/Applications
    cp -r Dolphin.app $out/Applications/
  '';
  meta = {
    description = "GameCube/Wii emulator";
    homepage = "https://dolphin-emu.org";
    platforms = lib.platforms.darwin;
    license = lib.licenses.gpl2Plus;
  };
}
