# Public package surface. Policy lives in the packaging modules; this file only composes them.
{ nixpkgsEsDe, btrc, forAllSystems, mkPkgs, ... }:

forAllSystems (system:
  let
    pkgs = mkPkgs system;
    lib = pkgs.lib;
    repositoryRoot = ../../..;
    btrcpy = btrc.packages.${system}.btrcpy;
    esDePkgs = import nixpkgsEsDe {
      inherit system;
      config.allowInsecurePredicate = pkg: lib.hasPrefix "freeimage" (lib.getName pkg);
    };
    semuProgram = pkgs.callPackage ../semu_program.nix { inherit btrcpy; };
    semuSource = pkgs.callPackage ../semu_source.nix { };
    semuCli = pkgs.callPackage ../semu_cli.nix { inherit semuProgram semuSource; };
    semuRenderer = pkgs.callPackage ../renderer.nix { inherit btrcpy repositoryRoot; };
    visualAssets = import ./visual-assets.nix { inherit pkgs lib repositoryRoot; };
    emulators = pkgs.callPackage ../emulators.nix { inherit repositoryRoot semuRenderer btrcpy; };
    retroarch = emulators.packages.retroarch;
    esDe = pkgs.callPackage (repositoryRoot + "/packaging/esde/package.nix") { esDePackages = esDePkgs; };
    semu = pkgs.callPackage ../semu_bundle.nix {
      inherit semuCli esDe repositoryRoot;
      emulatorPackages = lib.attrValues emulators.packages;
      extraPackages = [ pkgs.retroarch-joypad-autoconfig pkgs.syncthing semuRenderer visualAssets.combined ];
    };
    release = pkgs.callPackage ../release.nix { inherit semu repositoryRoot; };
  in {
    inherit btrcpy retroarch semu release;
    semu-renderer = semuRenderer;
    visual-assets = visualAssets.combined;
    default = semu;
    semu-program = semuProgram;
    semu-source = semuSource;
    semu-cli = semuCli;
    es-de = esDe;
  } // lib.mapAttrs' (id: package: lib.nameValuePair "emulator-${id}" package) emulators.packages)
