# Public package surface. Policy lives in the packaging modules; this file only composes them.
# Development outputs (the compiler, the CLI, the bezel tree) exist on every system; the product
# (bundle, emulators, release) only where it runs, Linux.
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
    bezelTree = pkgs.callPackage ../bezel_tree.nix { inherit repositoryRoot; };
    bezelLayers = pkgs.callPackage ../bezel_layers.nix { inherit repositoryRoot; };
    development = {
      inherit btrcpy;
      semu-program = semuProgram;
      semu-source = semuSource;
      semu-cli = semuCli;
      bezel-tree = bezelTree;
      bezel-layers = bezelLayers;
    };
    product =
      let
        semuRenderer = renderer.packages.${system}.default;
        visualAssets = import ./visual-assets.nix { inherit pkgs lib repositoryRoot; };
        emulators = import ../emulators.nix { inherit lib repositoryRoot system emulatorFlakes coreFlakes; retroarchFlake = retroarch; };
        esDe = esde.packages.${system}.default;
        semu = pkgs.callPackage ../semu_bundle.nix {
          inherit semuCli esDe repositoryRoot;
          emulatorPackages = lib.attrValues emulators.packages;
          extraPackages = [ pkgs.retroarch-joypad-autoconfig pkgs.syncthing semuRenderer visualAssets.combined bezelLayers ];
        };
        release = pkgs.callPackage ../release.nix { inherit semu repositoryRoot; };
      in {
        inherit semu release;
        retroarch = emulators.packages.retroarch;
        semu-renderer = semuRenderer;
        visual-assets = visualAssets.combined;
        bezel-generate = visualAssets.bezels.generate;  # re-renders recipe bezels with imagemagick; copy the output back into config/assets
        default = semu;
        es-de = esDe;
      } // lib.mapAttrs' (id: package: lib.nameValuePair "emulator-${id}" package) emulators.packages;
  in development // lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux product)
