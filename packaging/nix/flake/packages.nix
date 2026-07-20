# Public per-system package surface. Build policy remains in the owned package
# modules and emulator-local recipes; this file only composes those values.
{
  nixpkgsEsDe,
  btrc,
  nixGL,
  forAllSystems,
  mkPkgs,
  ...
}:

forAllSystems (
  system:
  let
    pkgs = mkPkgs system;
    lib = pkgs.lib;
    isDarwin = pkgs.stdenv.hostPlatform.isDarwin;
    isLinux = pkgs.stdenv.hostPlatform.isLinux;
    isSteamDeckBuild = system == "x86_64-linux";
    repositoryRoot = ../../..;
    btrcpy = btrc.packages.${system}.btrcpy;

    semuProgram =
      if isSteamDeckBuild then
        pkgs.pkgsStatic.callPackage ../semu_program.nix {
          inherit btrcpy;
          requireStaticBootstrap = true;
        }
      else
        pkgs.callPackage ../semu_program.nix {
          inherit btrcpy;
        };
    semuProgramLinux =
      if isSteamDeckBuild then
        semuProgram
      else
        pkgs.callPackage ../semu_program.nix {
          inherit btrcpy;
          crossTarget = "x86_64-linux-musl";
          zig = pkgs.zig;
        };
    semuSyncTemplate = pkgs.callPackage (repositoryRoot + "/packaging/sync/package.nix") { };
    semuSource = pkgs.callPackage ../semu_source.nix {
      inherit semuSyncTemplate;
    };
    semuCli = pkgs.callPackage ../semu_cli.nix {
      inherit semuProgram semuSource;
      syncthingtray = if isLinux then pkgs.syncthingtray else null;
      bubblewrap = if isLinux then pkgs.bubblewrap else null;
    };

    visualAssets = import ./visual-assets.nix {
      inherit pkgs lib repositoryRoot;
    };
    semuRenderer = pkgs.callPackage ../renderer.nix {
      inherit btrcpy repositoryRoot;
    };
    semuEmulators = pkgs.callPackage ../emulators.nix {
      inherit semuRenderer;
    };

    steamDeck =
      if isSteamDeckBuild then
        import ./steam-deck-package.nix {
          inherit
            pkgs
            lib
            system
            repositoryRoot
            nixpkgsEsDe
            nixGL
            semuProgram
            semuSource
            semuRenderer
            semuEmulators
            ;
          semuBezels = visualAssets.bezels;
          semuShaders = visualAssets.shaders;
        }
      else
        null;

    esDe =
      if steamDeck != null then
        steamDeck.esDe
      else if isDarwin then
        pkgs.callPackage (repositoryRoot + "/packaging/esde/package.nix") { }
      else
        null;

    darwinApp =
      if isDarwin then
        pkgs.callPackage ../semu_app.nix {
          inherit semuCli esDe;
          emulatorPackages = lib.attrValues semuEmulators.inventory;
          runtimeTools = [
            pkgs.syncthing
            pkgs.curl
            semuRenderer
          ];
          semuBezels = visualAssets.bezels;
          semuShaders = visualAssets.shaders;
        }
      else
        null;
  in
  {
    inherit btrcpy;
    default =
      if steamDeck != null then
        steamDeck.runtime
      else if darwinApp != null then
        darwinApp
      else
        semuCli;
    semu-program = semuProgram;
    semu-program-linux = semuProgramLinux;
    semu-source = semuSource;
    semu-sync-template = semuSyncTemplate;
    semu-cli = semuCli;
    semu-renderer = semuRenderer;
    semu-bezels = visualAssets.bezels;
    semu-bezels-generate = visualAssets.bezels.generate;
    semu-shaders = visualAssets.shaders;
    visual-assets = visualAssets.combined;
  }
  // lib.optionalAttrs (esDe != null) { es-de = esDe; }
  // lib.optionalAttrs (steamDeck != null) {
    steamdeck-runtime = steamDeck.runtime;
  }
  // lib.optionalAttrs isSteamDeckBuild (
    lib.mapAttrs' (
      id: package: lib.nameValuePair "${id}-runtime" package
    ) semuEmulators.runtimeInventory
  )
  // lib.optionalAttrs (isSteamDeckBuild && semuEmulators.retroarchCores != null) {
    retroarch-cores = semuEmulators.retroarchCores;
  }
  // lib.optionalAttrs isDarwin semuEmulators.inventory
)
