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

  # Coverage invariant of the two manifest interpreters: every asset entry in
  # bezels.json and shaders.json must be claimed by its interpreter.
  bezelManifest = lib.importJSON ../../../assets/bezels.json;
  shaderManifest = lib.importJSON ../../../assets/shaders.json;
  assetNames = lib.attrNames bezelManifest.assets
    ++ lib.attrNames shaderManifest.assets;
  coveredNames = semuBezels.imageAssetNames ++ semuShaders.shaderAssetNames;
  uncovered = lib.filter (name: !(lib.elem name coveredNames)) assetNames;

  visualAssets =
    assert lib.assertMsg (uncovered == [ ])
      "manifest assets with no nix interpreter: ${toString uncovered}";
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
  # stdenvNoCC interpreter for the shaders.json asset manifest while the tap is
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
          # Ship the btrc tap: transpile libsemutap.btrc -> C (--no-dce so the
          # library's exported hooks survive dead-code elimination), then link the
          # stb_image TU, restricting exports to the 4 contract symbols (else
          # btrc's external-linkage default leaks ~53 helper globals into the
          # LD_PRELOAD namespace). The C source (libsemutap.c) is retained in the
          # tree as an instant revert until this is Deck-smoke-tested at runtime.
          ${btrcpy}/bin/btrcpy libsemutap.btrc -o libsemutap_gen.c --no-dce --no-stdlib
          $CC -c -O2 stb_impl.c -o stb_impl.o
          cat > exports.map <<'MAP'
{ global: semu_tap_report; dlsym; glXSwapBuffers; eglSwapBuffers; local: *; };
MAP
          $CC -shared -fPIC -O2 -I. libsemutap_gen.c stb_impl.o \
            -Wl,--version-script=exports.map -ldl -lm -o libsemutap.so
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
  # The regenerator behind `nix run .#bake-bezels`: fetches upstreams + renders
  # bezels.json with imagemagick. Independently buildable for debugging; a
  # normal app/asset build never forces it (it hangs off semuBezels.passthru).
  semu-bezels-generate = semuBezels.generate;
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
