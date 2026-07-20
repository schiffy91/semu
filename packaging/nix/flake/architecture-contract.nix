{
  lib,
  systems,
  repositoryRoot,
}:

let
  directoriesWith =
    root: fileName:
    lib.attrNames (
      lib.filterAttrs (
        name: type: type == "directory" && builtins.pathExists (root + "/${name}/${fileName}")
      ) (builtins.readDir root)
    );

  targetsRoot = repositoryRoot + "/config/targets";
  targetFiles = lib.filterAttrs (name: type: type == "regular" && lib.hasSuffix ".json" name) (
    builtins.readDir targetsRoot
  );
  targetIds = map (name: lib.removeSuffix ".json" name) (lib.attrNames targetFiles);
  targets = lib.genAttrs targetIds (id: lib.importJSON (targetsRoot + "/${id}.json"));
  linuxTargetIds = lib.filter (id: (targets.${id}.platform or "") == "linux") targetIds;
  invalidTargetBackends = lib.concatMap (
    id:
    map (backend: "${id}:${backend}") (
      lib.filter (backend: backend != "bwrap") (targets.${id}.backends or [ ])
    )
  ) linuxTargetIds;
  targetSystems = lib.unique (lib.concatMap (id: (targets.${id}.nix_systems or [ ])) targetIds);
  unsupportedTargetSystems = lib.filter (system: !(lib.elem system systems)) targetSystems;

  emulatorsRoot = repositoryRoot + "/config/emulators";
  emulatorIds = directoriesWith emulatorsRoot "emulator.json";
  emulatorContracts = lib.genAttrs emulatorIds (
    id: lib.importJSON (emulatorsRoot + "/${id}/emulator.json")
  );
  linuxEmulatorIds = lib.filter (id: emulatorContracts.${id} ? platforms.linux) emulatorIds;
  linuxPlatform = id: emulatorContracts.${id}.platforms.linux;
  invalidLinuxBackends = lib.filter (
    id: ((linuxPlatform id).backend or "") != "bwrap"
  ) linuxEmulatorIds;
  nonBundledExecutables = lib.filter (
    id: !(lib.hasPrefix "\${asset_root}/" ((linuxPlatform id).executable or ""))
  ) linuxEmulatorIds;
  missingPackageRecipes = lib.filter (
    id: !(builtins.pathExists (emulatorsRoot + "/${id}/package.nix"))
  ) linuxEmulatorIds;
  missingPackageContracts = lib.filter (
    id: !(builtins.pathExists (emulatorsRoot + "/${id}/package.json"))
  ) linuxEmulatorIds;
  packageContracts = lib.genAttrs (lib.filter (
    id: !(lib.elem id missingPackageContracts)
  ) linuxEmulatorIds) (id: lib.importJSON (emulatorsRoot + "/${id}/package.json"));
  enabledFallbacks = lib.concatMap (
    id:
    let
      fallbacks = packageContracts.${id}.fallbacks or { };
    in
    map (name: "${id}:${name}") (
      lib.filter (name: fallbacks.${name} != false) (lib.attrNames fallbacks)
    )
  ) (lib.attrNames packageContracts);
  missingFallbackContracts = lib.filter (
    id: builtins.attrNames (packageContracts.${id}.fallbacks or { }) == [ ]
  ) (lib.attrNames packageContracts);
  incompleteSourcePins = lib.filter (
    id:
    let
      source = packageContracts.${id}.source or { };
    in
    !(source ? revision) || !(source ? sha256) || !(lib.hasPrefix "sha256-" source.sha256)
  ) (lib.attrNames packageContracts);

  flakeLock = lib.importJSON (repositoryRoot + "/flake.lock");
  btrcNodeName = flakeLock.nodes.root.inputs.btrc;
  btrcLock = flakeLock.nodes.${btrcNodeName}.locked;
  remoteBtrc =
    btrcLock.type == "github"
    && btrcLock.owner == "schiffy91"
    && btrcLock.repo == "btrc"
    && builtins.stringLength btrcLock.rev == 40;

  publicSurfaceFiles = [
    (repositoryRoot + "/flake.nix")
    ./packages.nix
    ./steam-deck-package.nix
  ];
  forbiddenSurfaceTerms = [
    "build/appimage"
    "prebuiltProgram"
    "nixosModules"
    "pkgs.dolphin-emu"
    "pkgs.pcsx2"
    "pkgs.cemu"
    "pkgs.retroarch-bare"
    "services.flatpak"
    "semu bootstrap"
  ];
  staleSurfaceReferences = lib.concatMap (
    file:
    map (term: "${builtins.baseNameOf file}:${term}") (
      lib.filter (term: lib.hasInfix term (builtins.readFile file)) forbiddenSurfaceTerms
    )
  ) publicSurfaceFiles;
in
assert lib.assertMsg (
  lib.sort builtins.lessThan systems == lib.sort builtins.lessThan [
    "aarch64-darwin"
    "x86_64-darwin"
    "x86_64-linux"
    "aarch64-linux"
  ]
) "the public flake system set changed without updating its contract";
assert lib.assertMsg (
  unsupportedTargetSystems == [ ]
) "targets declare unsupported Nix systems: ${toString unsupportedTargetSystems}";
assert lib.assertMsg (
  invalidTargetBackends == [ ]
) "Linux targets declare non-bwrap backends: ${toString invalidTargetBackends}";
assert lib.assertMsg (
  invalidLinuxBackends == [ ]
) "Linux emulator contracts declare non-bwrap backends: ${toString invalidLinuxBackends}";
assert lib.assertMsg (
  nonBundledExecutables == [ ]
) "Linux emulator executables escape the bundled asset root: ${toString nonBundledExecutables}";
assert lib.assertMsg (
  missingPackageRecipes == [ ]
) "Linux emulator contracts lack package.nix recipes: ${toString missingPackageRecipes}";
assert lib.assertMsg (
  missingPackageContracts == [ ]
) "Linux emulator contracts lack package.json: ${toString missingPackageContracts}";
assert lib.assertMsg (missingFallbackContracts == [ ])
  "Linux emulator packages must explicitly disable every fallback: ${toString missingFallbackContracts}";
assert lib.assertMsg (
  enabledFallbacks == [ ]
) "Linux emulator packages enable forbidden fallbacks: ${toString enabledFallbacks}";
assert lib.assertMsg (
  incompleteSourcePins == [ ]
) "Linux emulator source pins are incomplete: ${toString incompleteSourcePins}";
assert lib.assertMsg remoteBtrc
  "the BTRC input must be the revision-pinned schiffy91/btrc GitHub flake";
assert lib.assertMsg (
  !(builtins.pathExists (repositoryRoot + "/packaging/nix/module.nix"))
) "the unsupported legacy NixOS module was restored";
assert lib.assertMsg (
  staleSurfaceReferences == [ ]
) "the public Nix surface contains legacy fallbacks: ${toString staleSurfaceReferences}";
{
  schema_version = 1;
  inherit
    systems
    targetIds
    linuxTargetIds
    linuxEmulatorIds
    ;
  btrc_revision = btrcLock.rev;
}
