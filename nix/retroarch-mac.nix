# RetroArch for macOS — pre-built universal DMG from buildbot
# nixpkgs retroarch is marked broken on Darwin, so we package the official build.
# RetroArch has a built-in core updater for downloading individual emulator cores.
{ lib, stdenv, fetchurl, undmg }:

let
  version = "1.22.2";
in
stdenv.mkDerivation {
  pname = "retroarch";
  inherit version;
  src = fetchurl {
    url = "https://buildbot.libretro.com/stable/${version}/apple/osx/universal/RetroArch_Metal.dmg";
    hash = "sha256-gbeRIbom1TkGSuE7TQQZoSDD0WWvvmVs9fVBKxX9tDQ=";
  };
  sourceRoot = ".";
  nativeBuildInputs = [ undmg ];
  installPhase = ''
    mkdir -p $out/Applications
    cp -r RetroArch.app $out/Applications/
  '';
  meta = {
    description = "RetroArch — multi-system emulator frontend (Metal build)";
    homepage = "https://www.retroarch.com";
    platforms = [ "aarch64-darwin" "x86_64-darwin" ];
    license = lib.licenses.gpl3;
  };
}
