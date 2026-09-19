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
    emulators = pkgs.callPackage ../emulators.nix { inherit repositoryRoot; };
    retroarch = emulators.packages.retroarch;
    esDe = pkgs.callPackage (repositoryRoot + "/packaging/esde/package.nix") { esDePackages = esDePkgs; };
    semu = pkgs.callPackage ../semu_bundle.nix {
      inherit semuCli esDe repositoryRoot;
      emulatorPackages = lib.attrValues emulators.packages;
      extraPackages = [ pkgs.retroarch-joypad-autoconfig ];
    };
    release = pkgs.callPackage ../release.nix { inherit semu repositoryRoot; };
  in {
    inherit btrcpy retroarch semu release;
    default = semu;
    semu-program = semuProgram;
    semu-source = semuSource;
    semu-cli = semuCli;
    es-de = esDe;
  } // lib.mapAttrs' (id: package: lib.nameValuePair "emulator-${id}" package) emulators.packages)
