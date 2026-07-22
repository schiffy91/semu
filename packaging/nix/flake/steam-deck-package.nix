{
  pkgs,
  lib,
  system,
  repositoryRoot,
  nixpkgsEsDe,
  nixGL,
  semuProgram,
  semuSource,
  semuRenderer,
  semuEmulators,
  semuBezels,
  semuShaders,
  selectedEmulatorIds ? null,
  packageAttribute ? "steamdeck-runtime",
  releaseKind ? "aggregate",
}:

assert lib.assertMsg (
  system == "x86_64-linux"
) "the Steam Deck package can only be evaluated for x86_64-linux";

let
  semuCli = pkgs.callPackage ../semu_cli.nix {
    inherit semuProgram semuSource;
    syncthing = null;
    syncthingtray = null;
    curl = null;
    bubblewrap = null;
  };

  esDePackages = import nixpkgsEsDe {
    inherit system;
    config.allowInsecurePredicate = package: lib.getName package == "freeimage";
  };
  esDe = pkgs.callPackage (repositoryRoot + "/packaging/esde/package.nix") {
    inherit esDePackages;
    steamDeck = true;
  };

  nixGLMesa = import nixGL {
    inherit pkgs;
    enable32bits = false;
    enableIntelX86Extensions = false;
  };
  nixGLRuntime = pkgs.runCommand "semu-nixgl" { } ''
    mkdir -p "$out/bin"
    ln -s ${nixGLMesa.nixGLIntel}/bin/nixGLIntel "$out/bin/nixGL"
  '';

  appimageRuntime = pkgs.callPackage ../appimage_runtime.nix { };

  runtimeInventoryIds = lib.sort builtins.lessThan (
    builtins.attrNames semuEmulators.runtimeInventory
  );
  selectedIds =
    if selectedEmulatorIds == null then
      runtimeInventoryIds
    else
      lib.sort builtins.lessThan selectedEmulatorIds;
  selectedInventory = lib.getAttrs selectedIds semuEmulators.runtimeInventory;
  selectedRuntimeExtras = lib.unique (
    lib.concatMap (
      id: lib.attrValues semuEmulators.runtimeExtrasByEmulator.${id}
    ) selectedIds
  );
  releaseInventory = {
    schema_version = 1;
    kind = releaseKind;
    target = "steam-deck";
    platform = "linux";
    nix_system = system;
    package_attribute = packageAttribute;
    emulator_ids = selectedIds;
  };
  releaseInventoryPackage = pkgs.runCommand
    "semu-release-inventory-${packageAttribute}"
    { nativeBuildInputs = [ pkgs.jq ]; }
    ''
      mkdir -p "$out/share/semu"
      printf '%s\n' ${lib.escapeShellArg (builtins.toJSON releaseInventory)} \
        | jq --sort-keys . \
        > "$out/share/semu/appimage-release-inventory.json"
    '';
  appimagePackaging = pkgs.callPackage ../appimage_packaging.nix {
    inherit
      repositoryRoot
      semuProgram
      selectedIds
      packageAttribute
      ;
  };
  emulatorPackages =
    assert lib.assertMsg (
      runtimeInventoryIds == semuEmulators.requiredLinuxEmulatorIds
    ) "Steam Deck runtime inventory does not match selected Linux emulators";
    assert lib.assertMsg (
      selectedIds != [ ]
      && lib.unique selectedIds == selectedIds
      && lib.all (id: builtins.hasAttr id semuEmulators.runtimeInventory) selectedIds
    ) "Steam Deck release selects an invalid emulator package set";
    assert lib.assertMsg (
      (releaseKind == "aggregate" && selectedIds == runtimeInventoryIds)
      || (releaseKind == "emulator-slice"
        && builtins.length selectedIds == 1
        && packageAttribute == "steamdeck-runtime-${builtins.head selectedIds}")
    ) "Steam Deck release kind does not match its emulator package set";
    assert lib.assertMsg (
      (releaseKind == "aggregate" && packageAttribute == "steamdeck-runtime")
      || releaseKind == "emulator-slice"
    ) "Steam Deck release package attribute is invalid";
    assert lib.assertMsg (lib.all lib.isDerivation (
      lib.attrValues selectedInventory
    )) "Steam Deck runtime inventory contains a non-package value";
    assert lib.assertMsg (lib.all lib.isDerivation selectedRuntimeExtras)
      "Steam Deck emulator runtime extras contain a non-package value";
    lib.attrValues selectedInventory ++ selectedRuntimeExtras;
in
{
  inherit esDe;

  runtime = (pkgs.callPackage ../semu_app.nix {
    inherit
      semuCli
      esDe
      semuBezels
      semuShaders
      emulatorPackages
      ;
    runtimeTools = [
      pkgs.bash
      pkgs.bubblewrap
      pkgs.curl
      pkgs.syncthing
      nixGLRuntime
      appimageRuntime
      appimagePackaging
      semuRenderer
      releaseInventoryPackage
    ];
  }).overrideAttrs (previous: {
    name = "semu-app-${packageAttribute}";
    passthru = (previous.passthru or { }) // {
      semuBootstrapCli = semuProgram;
      semuAppImagePackaging = appimagePackaging;
      semuReleaseInventory = releaseInventory;
    };
  });
}
