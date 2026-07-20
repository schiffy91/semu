# semu_source.nix - immutable declarative configuration payload.
{ lib, stdenvNoCC, semuSyncTemplate }:

let
  repositoryRoot = ../..;
  runtimeSource = lib.fileset.toSource {
    root = repositoryRoot;
    fileset = repositoryRoot + "/config";
  };
in
stdenvNoCC.mkDerivation {
  pname = "semu-source";
  version = "0.1.0";
  src = runtimeSource;
  dontBuild = true;

  installPhase = ''
    mkdir -p "$out/share/semu"
    cp -r config "$out/share/semu/config"
    install -Dm0444 \
      "${semuSyncTemplate}/share/semu/sync/semu-syncthing.service.template" \
      "$out/share/semu/config/templates/sync/semu-syncthing.service.template"
  '';

  meta = {
    description = "Semu immutable configuration contracts and assets";
    license = lib.licenses.mit;
  };
}
