# Public package surface. Policy lives in the packaging modules; this file only composes them.
{ btrc, renderer, retroarch, esde, emulatorFlakes, coreFlakes, forAllSystems, mkPkgs, ... }:

forAllSystems (system:
  let
    pkgs = mkPkgs system;
    lib = pkgs.lib;
    repositoryRoot = ../../..;
    btrcpy = btrc.packages.${system}.btrcpy;
    semuProgram = pkgs.callPackage ../semu_program.nix { inherit btrcpy; };
    semuSource = pkgs.callPackage ../semu_source.nix { };
    semuCli = pkgs.callPackage ../semu_cli.nix { inherit semuProgram semuSource; };
    semuRenderer = renderer.packages.${system}.default;
    visualAssets = import ./visual-assets.nix { inherit pkgs lib repositoryRoot; };
    emulators = import ../emulators.nix { inherit lib repositoryRoot system emulatorFlakes coreFlakes; retroarchFlake = retroarch; };
    esDe = esde.packages.${system}.default;
    semu = pkgs.callPackage ../semu_bundle.nix {
      inherit semuCli esDe repositoryRoot;
      emulatorPackages = lib.attrValues emulators.packages;
      extraPackages = [ pkgs.retroarch-joypad-autoconfig pkgs.syncthing semuRenderer visualAssets.combined ];
    };
    release = pkgs.callPackage ../release.nix { inherit semu repositoryRoot; };
  in {
    inherit btrcpy semu release;
    retroarch = emulators.packages.retroarch;
    semu-renderer = semuRenderer;
    visual-assets = visualAssets.combined;
    bezel-generate = visualAssets.bezels.generate;  # re-renders recipe bezels with imagemagick; copy the output back into config/assets
    default = semu;
    semu-program = semuProgram;
    semu-source = semuSource;
    semu-cli = semuCli;
    es-de = esDe;
  } // lib.mapAttrs' (id: package: lib.nameValuePair "emulator-${id}" package) emulators.packages)
