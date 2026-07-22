# Generic emulator inventory derived from repository contracts. Emulator-local
# recipes own source pins, implementation maps, and runtime extras.
{
  lib,
  stdenv,
  callPackage,
  btrcpy,
  semuRenderer,
}:

let
  repositoryRoot = ../..;
  emulatorsDir = repositoryRoot + "/config/emulators";
  systemsDir = repositoryRoot + "/config/systems";

  contractIds =
    dir: fileName:
    lib.attrNames (
      lib.filterAttrs (
        name: type: type == "directory" && builtins.pathExists (dir + "/${name}/${fileName}")
      ) (builtins.readDir dir)
    );

  emulatorIds = contractIds emulatorsDir "emulator.json";
  emulatorContracts = lib.genAttrs emulatorIds (
    id: lib.importJSON (emulatorsDir + "/${id}/emulator.json")
  );
  packageContracts = lib.genAttrs emulatorIds (
    id:
    let
      contract = emulatorsDir + "/${id}/package.json";
    in
    if builtins.pathExists contract then
      lib.importJSON contract
    else
      throw "semu_emulators.nix: missing config/emulators/${id}/package.json"
  );
  systemContracts = map (id: lib.importJSON (systemsDir + "/${id}/system.json")) (
    contractIds systemsDir "system.json"
  );
  targetPlatform =
    if stdenv.hostPlatform.isDarwin then
      "macos"
    else if stdenv.hostPlatform.isLinux then
      "linux"
    else
      stdenv.hostPlatform.system;

  disabledFallbacks =
    fallbacks:
    builtins.isAttrs fallbacks
    && builtins.attrNames fallbacks != [ ]
    && lib.all (name: fallbacks.${name} == false) (builtins.attrNames fallbacks);

  coreHostEnabled =
    contract:
    contract ? core_host
    && builtins.isAttrs contract.core_host
    && (contract.core_host.enabled or false);

  selectorValues =
    {
      hostId,
      selectorField,
      platform,
      systems,
    }:
    lib.sort builtins.lessThan (
      lib.unique (
        lib.concatMap (
          system:
          map (entry: entry.${selectorField}) (
            lib.filter (
              entry:
              (entry.emulator or "") == hostId
              && builtins.hasAttr selectorField entry
              && lib.elem platform (entry.platforms or [ platform ])
            ) (system.emulators or [ ])
          )
        ) systems
      )
    );

  coreHostIds = lib.attrNames (lib.filterAttrs (_: coreHostEnabled) packageContracts);
  coreHostSelections = lib.genAttrs coreHostIds (
    id:
    selectorValues {
      hostId = id;
      selectorField = packageContracts.${id}.core_host.selector_field;
      platform = targetPlatform;
      systems = systemContracts;
    }
  );

  renamedCoreHostEvaluation =
    selectorValues {
      hostId = "renamed-host";
      selectorField = "implementation";
      platform = "linux";
      systems = [
        {
          emulators = [
            {
              emulator = "renamed-host";
              implementation = "fixture_core";
              platforms = [ "linux" ];
            }
            {
              emulator = "unrelated-host";
              implementation = "wrong_core";
              platforms = [ "linux" ];
            }
          ];
        }
      ];
    } == [ "fixture_core" ];

  packageContractErrors = lib.concatMap (
    id:
    let
      contract = packageContracts.${id};
      source = contract.source or { };
      recipe = emulatorsDir + "/${id}/package.nix";
      recipeParameters =
        if builtins.pathExists recipe then builtins.functionArgs (import recipe) else { };
      prebuiltFields = lib.filter (name: lib.hasInfix "prebuilt" (lib.toLower name)) (
        builtins.attrNames contract
      );
      runtimeExtras = contract.runtime_extras or { };
      invalidRuntimeExtras = lib.filter (
        name:
        let
          extra = runtimeExtras.${name};
        in
        !builtins.isAttrs extra
        || !(builtins.isString (extra.kind or null))
        || !(builtins.isList (extra.platforms or null))
        || extra.platforms == [ ]
      ) (builtins.attrNames runtimeExtras);
      problems =
        lib.optional ((contract.build_kind or "") != "source") "build_kind"
        ++ lib.optional (
          !(builtins.isString (source.revision or null))
          || builtins.match "^[0-9a-f]{40}$" source.revision == null
        ) "source.revision"
        ++ lib.optional (
          !(builtins.isString (source.sha256 or null)) || !lib.hasPrefix "sha256-" source.sha256
        ) "source.sha256"
        ++ lib.optional (
          !(builtins.isList (contract.platforms or null)) || contract.platforms == [ ]
        ) "platforms"
        ++ lib.optional (!disabledFallbacks (contract.fallbacks or { })) "fallbacks"
        ++ lib.optional (prebuiltFields != [ ]) "active prebuilt field ${toString prebuiltFields}"
        ++ lib.optional (!(builtins.pathExists recipe)) "package.nix"
        ++ lib.optional (
          coreHostEnabled contract
          && (
            !(builtins.isString (contract.core_host.selector_field or null))
            || contract.core_host.selector_field == ""
            || !(recipeParameters ? coreHostId)
            || !(recipeParameters ? selectedCoreIds)
          )
        ) "core_host"
        ++ lib.optional (
          invalidRuntimeExtras != [ ]
        ) "invalid runtime extras ${toString invalidRuntimeExtras}";
    in
    map (problem: "${id}:${problem}") problems
  ) emulatorIds;

  packageIdsFor =
    platform: backends:
    lib.attrNames (
      lib.filterAttrs (
        _: contract:
        contract ? platforms.${platform} && lib.elem (contract.platforms.${platform}.backend or "") backends
      ) emulatorContracts
    );

  macIds = packageIdsFor "macos" [
    "nix"
    "native"
  ];
  linuxRuntimeIds = packageIdsFor "linux" [
    "bwrap"
    "native"
    "nix"
  ];

  emulatorsFor =
    platform:
    lib.unique (
      lib.concatMap (
        system:
        map (entry: entry.emulator) (
          lib.filter (entry: entry ? emulator && lib.elem platform (entry.platforms or [ platform ])) (
            system.emulators or [ ]
          )
        )
      ) systemContracts
    );
  requiredLinuxEmulatorIds = lib.sort builtins.lessThan (emulatorsFor "linux");
  missingLinuxRuntimeIds = lib.subtractLists linuxRuntimeIds requiredLinuxEmulatorIds;

  recipeArguments =
    id: recipe:
    let
      arguments = builtins.functionArgs (import recipe);
      contract = packageContracts.${id};
    in
    lib.optionalAttrs (arguments ? semuRenderer) { inherit semuRenderer; }
    // lib.optionalAttrs (arguments ? btrcpy) { inherit btrcpy; }
    // lib.optionalAttrs (coreHostEnabled contract) {
      coreHostId = id;
      selectedCoreIds = coreHostSelections.${id};
    };

  emulatorPackage =
    id:
    let
      recipe = emulatorsDir + "/${id}/package.nix";
    in
    if builtins.pathExists recipe then
      callPackage recipe (recipeArguments id recipe)
    else
      throw (
        "semu_emulators.nix: no recipe config/emulators/${id}/package.nix" + " for emulator '${id}'"
      );

  inventory = lib.optionalAttrs stdenv.hostPlatform.isDarwin (lib.genAttrs macIds emulatorPackage);
  runtimeInventory = lib.optionalAttrs stdenv.hostPlatform.isLinux (
    lib.genAttrs linuxRuntimeIds emulatorPackage
  );

  declaredRuntimeExtrasFor =
    id:
    lib.filterAttrs (_: extra: lib.elem stdenv.hostPlatform.system extra.platforms) (
      packageContracts.${id}.runtime_extras or { }
    );
  runtimeExtrasFor =
    id: package:
    let
      expectedIds = lib.sort builtins.lessThan (builtins.attrNames (declaredRuntimeExtrasFor id));
      provided = package.semuRuntimeExtras or { };
      providedIds = lib.sort builtins.lessThan (builtins.attrNames provided);
    in
    assert lib.assertMsg (
      providedIds == expectedIds
    ) "semu_emulators.nix: runtime extras for '${id}' do not match its declaration";
    assert lib.assertMsg (lib.all lib.isDerivation (
      lib.attrValues provided
    )) "semu_emulators.nix: runtime extras for '${id}' contain a non-package value";
    provided;
  runtimeExtrasByEmulator = lib.mapAttrs runtimeExtrasFor runtimeInventory;
  runtimeExtraPackages = lib.unique (
    lib.concatMap lib.attrValues (lib.attrValues runtimeExtrasByEmulator)
  );

  activeCoreHostIds = lib.filter (id: builtins.hasAttr id runtimeInventory) coreHostIds;
  coreHostPackages = lib.filterAttrs (_: package: package != null) (
    lib.genAttrs activeCoreHostIds (id: runtimeInventory.${id}.semuCoreHostPackage or null)
  );
  missingCoreHostPackages = lib.filter (
    id: coreHostSelections.${id} == [ ] || !(builtins.hasAttr id coreHostPackages)
  ) activeCoreHostIds;
  coreHostContract = {
    schema_version = 1;
    host_ids = coreHostIds;
    selectors = lib.genAttrs coreHostIds (id: {
      field = packageContracts.${id}.core_host.selector_field;
      selected = coreHostSelections.${id};
    });
    renamed_host_evaluation = renamedCoreHostEvaluation;
  };

  hostNamedCorePackages = lib.mapAttrs' (
    id: package: lib.nameValuePair "${id}Cores" package
  ) coreHostPackages;
in
assert lib.assertMsg (
  packageContractErrors == [ ]
) "semu_emulators.nix: invalid declarative source slices: ${toString packageContractErrors}";
assert lib.assertMsg renamedCoreHostEvaluation
  "semu_emulators.nix: core-host selection depends on a concrete emulator identity";
assert lib.assertMsg (missingLinuxRuntimeIds == [ ])
  "semu_emulators.nix: selected Linux emulators lack packaged runtime slices: ${toString missingLinuxRuntimeIds}";
assert lib.assertMsg (missingCoreHostPackages == [ ])
  "semu_emulators.nix: selected core hosts lack local package outputs: ${toString missingCoreHostPackages}";
{
  declaredIds = emulatorIds;
  inherit
    inventory
    runtimeInventory
    requiredLinuxEmulatorIds
    macIds
    coreHostContract
    coreHostPackages
    runtimeExtrasByEmulator
    runtimeExtraPackages
    ;
}
// hostNamedCorePackages
