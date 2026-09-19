# Every emulator the linux system bindings select, built through config/emulators/<id>/package.nix.
{ lib, callPackage, repositoryRoot, semuRenderer, btrcpy }:

let
  systemsDir = repositoryRoot + "/config/systems";
  emulatorsDir = repositoryRoot + "/config/emulators";
  systemIds = lib.attrNames (lib.filterAttrs (_: type: type == "directory") (builtins.readDir systemsDir));
  systemContracts = map (id: lib.importJSON (systemsDir + "/${id}/system.json")) systemIds;
  linuxEmulators = lib.unique (lib.concatMap (system:
    map (entry: entry.emulator) (lib.filter (entry: entry ? emulator && lib.elem "linux" (entry.platforms or [ "linux" ])) (system.emulators or [ ]))
  ) systemContracts);
  recipe = id:
    let path = emulatorsDir + "/${id}/package.nix";
    in if builtins.pathExists path
       then callPackage path (lib.intersectAttrs (lib.functionArgs (import path)) { inherit semuRenderer btrcpy; })  # recipes take only what they declare
       else throw "emulators.nix: config/emulators/${id}/package.nix is missing";
in {
  ids = linuxEmulators;
  packages = lib.genAttrs linuxEmulators recipe;
}
