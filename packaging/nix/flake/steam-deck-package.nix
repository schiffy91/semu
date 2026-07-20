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
  emulatorPackages =
    assert lib.assertMsg (
      runtimeInventoryIds == semuEmulators.requiredLinuxEmulatorIds
    ) "Steam Deck runtime inventory does not match selected Linux emulators";
    assert lib.assertMsg (lib.all lib.isDerivation (
      lib.attrValues semuEmulators.runtimeInventory
    )) "Steam Deck runtime inventory contains a non-package value";
    assert lib.assertMsg (lib.all lib.isDerivation semuEmulators.runtimeExtraPackages)
      "Steam Deck emulator runtime extras contain a non-package value";
    lib.attrValues semuEmulators.runtimeInventory ++ semuEmulators.runtimeExtraPackages;
in
{
  inherit esDe;

  runtime = pkgs.callPackage ../semu_app.nix {
    inherit
      semuCli
      esDe
      semuBezels
      semuShaders
      emulatorPackages
      ;
    runtimeTools = [
      pkgs.bash
      pkgs.curl
      pkgs.syncthing
      nixGLRuntime
      appimageRuntime
      semuRenderer
    ];
  };
}
