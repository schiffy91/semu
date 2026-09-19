# RetroArch with exactly the cores the linux system bindings select.
{ lib, retroarch, libretro, repositoryRoot }:

let
  systemsDir = repositoryRoot + "/config/systems";
  coreManifest = lib.importJSON (repositoryRoot + "/config/emulators/retroarch/cores.json");
  systemIds = lib.attrNames (lib.filterAttrs (_: type: type == "directory") (builtins.readDir systemsDir));
  systemContracts = map (id: lib.importJSON (systemsDir + "/${id}/system.json")) systemIds;
  linuxBinding = entry: (entry.emulator or "") == "retroarch" && entry ? core && lib.elem "linux" (entry.platforms or [ "linux" ]);
  selectedCores = lib.unique (lib.concatMap (system: map (entry: entry.core) (lib.filter linuxBinding (system.emulators or [ ]))) systemContracts);
  corePackage = core:
    let
      implementation = coreManifest.implementations.${core}.linux or (throw "retroarch.nix: core '${core}' has no linux implementation in cores.json");
      attribute = implementation.package_attribute;
    in
    assert lib.assertMsg (implementation.kind == "nixpkgs_libretro") "retroarch.nix: core '${core}' is not a nixpkgs libretro core";
    libretro.${attribute} or (throw "retroarch.nix: nixpkgs libretro has no '${attribute}' for core '${core}'");
in
(retroarch.withCores (_: map corePackage selectedCores)).overrideAttrs (previous: {
  passthru = (previous.passthru or { }) // { semuSelectedCores = selectedCores; };
})
