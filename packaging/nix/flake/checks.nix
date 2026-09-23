# Flake checks: every package evaluates, the contract tests pass, and real RetroArch runs headless.
{ self, forAllSystems, mkPkgs, renderer, retroarch, esde, emulatorFlakes, coreFlakes, ... }:

forAllSystems (system:
  let
    pkgs = mkPkgs system;
    lib = pkgs.lib;
    packages = self.packages.${system};
    repositoryRoot = ../../..;
    contracts = pkgs.stdenv.mkDerivation {
      name = "semu-contracts";
      src = lib.fileset.toSource {
        root = repositoryRoot;
        fileset = lib.fileset.unions [ ../../../src ../../../tests/contracts ../../../config ../../../packaging ];  # the specs read the ES-DE patch and package
      };
      nativeBuildInputs = [ packages.btrcpy ];
      SEMU_BEZEL_TREE = "${packages.bezel-tree}/share/semu/bezel/shaders";  # the placement and layer contracts run from pinned inputs
      dontConfigure = true;
      buildPhase = ''
        btrcpy tests/contracts/main.btrc -o contracts.c --strict-imports --no-cache --no-stdlib
        $CC contracts.c -std=c11 -O1 -Isrc/launch -o contracts -lm
      '';
      installPhase = ''
        export HOME="$TMPDIR/home"
        mkdir -p "$HOME"
        SEMU_PROJECT="$PWD" ./contracts
        touch "$out"
      '';
    };
    syntheticCore = pkgs.stdenv.mkDerivation {
      name = "semu-synthetic-libretro";
      src = lib.fileset.toSource {
        root = repositoryRoot;
        fileset = ../../../tests/integration/synthetic_core.btrc;
      };
      nativeBuildInputs = [ packages.btrcpy ];
      dontConfigure = true;
      buildPhase = ''
        btrcpy tests/integration/synthetic_core.btrc -o core.c --strict-imports --no-cache --no-stdlib --no-dce
        $CC core.c -std=gnu11 -O2 -shared -fPIC -I${retroarch.sourceTree}/libretro-common/include -o synthetic_libretro.so
      '';
      installPhase = ''
        install -Dm644 synthetic_libretro.so "$out/lib/retroarch/cores/synthetic_libretro.so"
      '';
    };
    retroarchHeadless = pkgs.stdenv.mkDerivation {
      name = "semu-retroarch-headless";
      src = lib.fileset.toSource {
        root = repositoryRoot;
        fileset = ../../../tests/integration/retroarch-headless.sh;
      };
      nativeBuildInputs = [ pkgs.xvfb-run pkgs.socat pkgs.imagemagick pkgs.mesa pkgs.findutils pkgs.gawk ];
      dontConfigure = true;
      dontBuild = true;
      installPhase = ''
        export SEMU_CLI="${packages.semu-cli}/bin/semu"
        export RETROARCH="${packages.retroarch}/bin/retroarch"
        export CORE="${syntheticCore}/lib/retroarch/cores/synthetic_libretro.so"
        export LIBGL_DRIVERS_PATH="${pkgs.mesa}/lib/dri"
        export __EGL_VENDOR_LIBRARY_DIRS="${pkgs.mesa}/share/glvnd/egl_vendor.d"
        export __GLX_VENDOR_LIBRARY_NAME=mesa
        export LD_LIBRARY_PATH="${pkgs.mesa}/lib"
        xvfb-run --auto-servernum --server-args="-screen 0 1280x800x24" sh tests/integration/retroarch-headless.sh
        touch "$out"
      '';
    };
    installer = pkgs.stdenv.mkDerivation {
      name = "semu-installer-contract";
      src = lib.fileset.toSource {
        root = repositoryRoot;
        fileset = lib.fileset.unions [ ../../../tests/integration/installer.sh ../../../packaging/deck/install.sh ];
      };
      nativeBuildInputs = [ pkgs.zstd pkgs.gnutar ];
      dontConfigure = true;
      dontBuild = true;
      installPhase = ''
        INSTALLER="$PWD/packaging/deck/install.sh" sh tests/integration/installer.sh
        touch "$out"
      '';
    };
    platformMatrix =  # every sub-flake declares linux, macos and windows; linux and macos packages must evaluate
      let
        flakes = { inherit renderer retroarch esde; }
          // lib.mapAttrs' (id: flake: lib.nameValuePair "emulator-${id}" flake) emulatorFlakes
          // lib.mapAttrs' (id: flake: lib.nameValuePair "core-${id}" flake) coreFlakes;
        emulatorDirectory = name:  # the emulator.json and package.json an emulator flake builds for, or null
          if name == "retroarch" then repositoryRoot + "/config/emulators/retroarch"
          else if lib.hasPrefix "emulator-" name then repositoryRoot + "/config/emulators/${lib.removePrefix "emulator-" name}"
          else null;
        row = name: flake:
          let
            meta = flake.semu; linux = flake.packages.x86_64-linux.default; inner = linux.passthru.unwrapped or linux;
            directory = emulatorDirectory name;
            slices = if directory == null then null else lib.attrNames ((lib.importJSON (directory + "/emulator.json")).platforms or { });
            declaredSlices = lib.filter (os: meta.platforms.${os} == true) [ "linux" "macos" "windows" ];
            packageSystems = if directory == null then null else lib.sort lib.lessThan ((lib.importJSON (directory + "/package.json")).platforms or [ ]);
          in
          assert lib.assertMsg (meta.platforms.linux == true && flake.packages ? x86_64-linux) "${name}: needs a linux package";
          assert lib.assertMsg (meta.platforms.windows == "planned") "${name}: windows must be declared as planned until it is built";
          assert lib.assertMsg (meta.platforms.macos == (flake.packages ? aarch64-darwin)) "${name}: the macos flag must match its aarch64-darwin package";
          assert lib.assertMsg ((inner.allowSubstitutes or true) == false) "${name}: must be compiled here, never substituted";
          assert lib.assertMsg (slices == null || lib.sort lib.lessThan slices == declaredSlices)
            "${name}: emulator.json platform slices [${lib.concatStringsSep " " (lib.sort lib.lessThan slices)}] must be the flake's semu.platforms [${lib.concatStringsSep " " declaredSlices}]";
          assert lib.assertMsg (packageSystems == null || packageSystems == lib.attrNames flake.packages)
            "${name}: package.json platforms [${lib.concatStringsSep " " packageSystems}] must be the flake's package systems [${lib.concatStringsSep " " (lib.attrNames flake.packages)}]";
          {
            inherit (meta) platforms;
            inherit slices;
            linux = builtins.unsafeDiscardStringContext linux.drvPath;  # evaluation is the proof; the check must not build Darwin here
            macos = if meta.platforms.macos then builtins.unsafeDiscardStringContext flake.packages.aarch64-darwin.default.drvPath else null;
            source = meta.source or null;
          };
      in pkgs.writeText "semu-platform-matrix.json" (builtins.toJSON (lib.mapAttrs row flakes));
  in { inherit contracts; } // lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
    inherit installer;
    platform-matrix = platformMatrix;
    synthetic-core = syntheticCore;
    retroarch-headless = retroarchHeadless;
    semu = packages.semu;
    retroarch = packages.retroarch;
    es-de = packages.es-de;
  })
