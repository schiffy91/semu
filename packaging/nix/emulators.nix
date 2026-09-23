# Every emulator the system bindings select for one platform ("linux" or "macos"), taken
# from its own flake. RetroArch is wrapped with exactly the core flakes the bindings name;
# on macOS the cores are separate packages that the bundle lays out beside it.
{ lib, repositoryRoot, system, platform ? "linux", emulatorFlakes, coreFlakes, retroarchFlake }:

let
  systemsDir = repositoryRoot + "/config/systems";
  systemIds = lib.attrNames (lib.filterAttrs (_: type: type == "directory") (builtins.readDir systemsDir));
  systemContracts = map (id: lib.importJSON (systemsDir + "/${id}/system.json")) systemIds;
  platformEntries = lib.concatMap (contract: lib.filter (entry: entry ? emulator && lib.elem platform (entry.platforms or [ platform ])) (contract.emulators or [ ])) systemContracts;  # no list: every platform
  platformEmulators = lib.unique (map (entry: entry.emulator) platformEntries);
  selectedCores = lib.unique (map (entry: entry.core) (lib.filter (entry: entry.emulator == "retroarch" && entry ? core) platformEntries));
  builtOnlyHere = id: package:  # every emulator is compiled from its flake's pin, never substituted
    let inner = package.passthru.unwrapped or package;
    in assert lib.assertMsg ((inner.allowSubstitutes or true) == false) "emulators.nix: ${id} would be substituted from a cache; its flake must set allowSubstitutes = false";
    package;
  corePackage = core:
    let flake = coreFlakes.${core} or (throw "emulators.nix: no core flake input for '${core}' (config/emulators/retroarch/cores/${core})");
    in builtOnlyHere "core ${core}" flake.packages.${system}.default;
  emulatorPackage = id:
    if id == "retroarch" then retroarchFlake.packages.${system}.default.passthru.withCores (map corePackage selectedCores)
    else (emulatorFlakes.${id} or (throw "emulators.nix: no emulator flake input for '${id}' (config/emulators/${id})")).packages.${system}.default;
in {
  ids = platformEmulators;
  cores = selectedCores;
  packages = lib.genAttrs platformEmulators (id: builtOnlyHere id (emulatorPackage id));
  corePackages = if platform == "macos" && lib.elem "retroarch" platformEmulators then map corePackage selectedCores else [ ];  # lib/retroarch/cores/*.dylib
}
