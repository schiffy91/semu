# The immutable declarative configuration payload.
{ lib, stdenvNoCC }:

let
  repositoryRoot = ../..;
  runtimeSource = lib.fileset.toSource {
    root = repositoryRoot;
    fileset = repositoryRoot + "/config";
  };
in
stdenvNoCC.mkDerivation {
  pname = "semu-source";
  version = "0.2.0";
  src = runtimeSource;
  dontBuild = true;

  installPhase = ''
    mkdir -p "$out/share/semu"
    cp -r config "$out/share/semu/config"
  '';

  meta = {
    description = "Semu declarative configuration and assets";
    license = lib.licenses.mit;
  };
}
