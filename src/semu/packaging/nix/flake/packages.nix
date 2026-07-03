# flake/packages.nix — wires the packaging recipes into per-system package
# sets. The bundle targets are macOS (mac emulator packages from the
# contracts) and x86_64-linux (the Deck: flatpak emulators at runtime, so the
# bundle carries CLI + ES-DE + libretro cores + assets); everything else gets
# the bare CLI as default.
{ self, nixpkgs, btrc, nixGL, systems, forAllSystems, mkPkgs, ... }:

forAllSystems (system: let
  pkgs = mkPkgs system;
  lib = pkgs.lib;
  isDarwin = pkgs.stdenv.hostPlatform.isDarwin;
  isLinux = pkgs.stdenv.hostPlatform.isLinux;
  isX86Linux = system == "x86_64-linux";
  isBundleTarget = isDarwin || isX86Linux;

  btrcpy = btrc.packages.${system}.btrcpy;

  semuCli = pkgs.callPackage ../semu_cli.nix {
    inherit btrcpy;
    syncthingtray = if isLinux then pkgs.syncthingtray else null;
    bubblewrap = if isLinux then pkgs.bubblewrap else null;
  };

  semuBezels = pkgs.callPackage ../../../assets/bezels.nix { };
  semuShaders = pkgs.callPackage ../../../assets/shaders.nix { };

  # Coverage invariant of the two sources.json interpreters: every asset
  # entry must be claimed by exactly one of them.
  sourceManifest = lib.importJSON ../../../assets/sources.json;
  assetNames = lib.attrNames sourceManifest.assets;
  coveredNames = semuBezels.imageAssetNames ++ semuShaders.shaderAssetNames;
  uncovered = lib.filter (name: !(lib.elem name coveredNames)) assetNames;

  visualAssets =
    assert lib.assertMsg (uncovered == [ ])
      "sources.json assets with no nix interpreter: ${toString uncovered}";
    pkgs.symlinkJoin {
      name = "semu-visual-assets";
      paths = [ semuBezels semuShaders ];
      passthru = {
        inherit assetNames;
        assetCount = lib.length assetNames;
      };
    };

  # libsemutap — the GL tap compositor (sources: src/semu/emulators/rendering/tap).
  # Judgment call: a stanza here instead of assets/shaders.nix, because that file is a
  # stdenvNoCC interpreter for the sources.json asset manifest while the tap is
  # compiled code needing a real toolchain. x86_64-linux only (the Deck: LD_PRELOAD
  # into the emulator process); the darwin plan is documented in rendering/tap/readme.md.
  semuTap =
    if isX86Linux then
      pkgs.stdenv.mkDerivation {
        pname = "libsemutap";
        version = "1";
        src = ../../../emulators/rendering/tap;
        dontConfigure = true;
        buildPhase = ''
          runHook preBuild
          $CC -shared -fPIC -O2 -o libsemutap.so libsemutap.c -ldl -lm
          runHook postBuild
        '';
        installPhase = ''
          runHook preInstall
          mkdir -p "$out/lib"
          cp libsemutap.so "$out/lib/"
          runHook postInstall
        '';
        meta = {
          description = "Semu GL tap compositor loaded into emulator processes";
          platforms = [ "x86_64-linux" ];
        };
      }
    else null;

  semuEmulators = pkgs.callPackage ../../../emulators/emulators.nix { };
  # meta.broken marks emulators whose upstream artifact is currently
  # unfetchable (dead pin); they stay addressable as .#<id> but drop out of
  # the composed bundle until re-pinned.
  emulatorPackages = lib.filter (package: !(package.meta.broken or false))
    (lib.attrValues semuEmulators.inventory)
    ++ lib.optional (semuEmulators.retroarchCores != null)
      semuEmulators.retroarchCores;

  esDe =
    if isX86Linux then pkgs.callPackage ../../../emulators/es_de/es_de.nix { steamDeck = true; }
    else if isDarwin then pkgs.callPackage ../../../emulators/es_de/es_de.nix { }
    else null;

  nixGLIntel = if isX86Linux then nixGL.packages.${system}.nixGLIntel else null;
  runtimeTools = [ pkgs.syncthing pkgs.curl ]
    ++ lib.optionals isLinux [ pkgs.syncthingtray pkgs.bubblewrap ]
    ++ lib.optional (nixGLIntel != null) nixGLIntel;

  semuApp = pkgs.callPackage ../semu_app.nix {
    inherit semuCli esDe semuBezels semuShaders emulatorPackages runtimeTools;
  };
in
{
  inherit btrcpy visualAssets;
  semu-cli = semuCli;
  semu-bezels = semuBezels;
  semu-shaders = semuShaders;
  default = if isBundleTarget then semuApp else semuCli;
}
// lib.optionalAttrs (esDe != null) { es-de = esDe; }
// lib.optionalAttrs (semuTap != null) { semu-tap = semuTap; }
// lib.optionalAttrs (semuEmulators.retroarchCores != null) {
  retroarch-cores = semuEmulators.retroarchCores;
}
# targeted builds of the contract-derived mac emulator packages
// semuEmulators.inventory)
