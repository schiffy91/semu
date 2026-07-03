# emulators.nix — emulator inventory derived from the JSON contracts.
#
# WHICH emulators need a Nix package comes from
# src/semu/emulators/<id>/emulator.json: a macos platform with backend
# "nix"/"native" needs a recipe; linux platforms declare flatpak ids, which
# are runtime concerns and never packaged here. WHICH libretro cores
# RetroArch carries comes from src/semu/systems/*/system.json emulator
# entries. HOW each emulator is built — source pins, hashes, patches,
# wrappers — lives next to its contract in
# src/semu/emulators/<id>/<id>.nix; this file only discovers the inventory
# and callPackages those recipes, so zero pins live here. A contract entry
# with no recipe file fails evaluation loudly.
{ lib, stdenv, callPackage, symlinkJoin, libretro }:

let
  emulatorsDir = ./.;
  systemsDir = ../systems;

  contractIds = dir: fileName: lib.attrNames (lib.filterAttrs
    (name: type: type == "directory"
      && builtins.pathExists (dir + "/${name}/${fileName}"))
    (builtins.readDir dir));

  emulatorContracts = lib.genAttrs (contractIds emulatorsDir "emulator.json")
    (id: lib.importJSON (emulatorsDir + "/${id}/emulator.json"));

  macIds = lib.attrNames (lib.filterAttrs
    (_: contract: contract ? platforms.macos
      && lib.elem (contract.platforms.macos.backend or "") [ "nix" "native" ])
    emulatorContracts);

  systemContracts = map (id: lib.importJSON (systemsDir + "/${id}/system.json"))
    (contractIds systemsDir "system.json");

  # Every libretro core any system routes to RetroArch on the platform
  # (entries without a "platforms" filter apply everywhere).
  coresFor = platform: lib.unique (lib.concatMap
    (system: map (entry: entry.core) (lib.filter
      (entry: (entry.emulator or "") == "retroarch" && entry ? core
        && lib.elem platform (entry.platforms or [ platform ]))
      (system.emulators or [ ])))
    systemContracts);

  # id-routed recipe dispatch: the aggregator knows WHICH extra inputs a
  # recipe needs beyond nixpkgs (only retroarch takes the contract-derived
  # core list); everything else about the build is the recipe's business.
  recipeArguments = id: lib.optionalAttrs (id == "retroarch") {
    cores = coresFor "macos";
  };

  emulatorPackage = id:
    let recipe = emulatorsDir + "/${id}/${id}.nix";
    in if builtins.pathExists recipe
      then callPackage recipe (recipeArguments id)
      else throw ("semu_emulators.nix: no recipe src/semu/emulators/${id}/${id}.nix"
        + " for emulator '${id}' (src/semu/emulators/${id}/emulator.json)");

  # --- Linux: flatpak emulators, but the Deck flatpak RetroArch loads the
  # contract cores from ${nix_result}/lib/retroarch/cores -------------------
  linuxCoreAttr = {
    gambatte = "gambatte";
    mgba = "mgba";
    genesis_plus_gx = "genesis-plus-gx";
    snes9x = "snes9x";
    mesen = "mesen";
    mupen64plus_next = "mupen64plus";
    desmume = "desmume";
    mednafen_psx = "beetle-psx";
    mednafen_psx_hw = "beetle-psx-hw";
    ppsspp = "ppsspp";
    flycast = "flycast";
  };
  linuxCore = core: libretro.${linuxCoreAttr.${core} or (throw
    "semu_emulators.nix: no nixpkgs libretro mapping for core '${core}'")};
  linuxCores = coresFor "linux";
in
{
  # id -> package for every emulator the macOS contracts require; {} on Linux.
  inventory = lib.optionalAttrs stdenv.hostPlatform.isDarwin
    (lib.genAttrs macIds emulatorPackage);

  retroarchCores =
    if stdenv.hostPlatform.isLinux && linuxCores != [ ] then
      symlinkJoin {
        name = "semu-retroarch-cores";
        paths = map linuxCore linuxCores;
      }
    else null;
}
