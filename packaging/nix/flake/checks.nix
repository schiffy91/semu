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
        fileset = lib.fileset.unions [ ../../../btrc.toml ../../../input_codes_abi.h ../../../PLAN.md ../../../src ../../../tests ../../../config ../../../packaging ];  # the specs read the plan's Status block, the ES-DE patch and package, the Deck scripts' plan and display rule, and the integration and visual harnesses' own text
      };
      nativeBuildInputs = [ packages.btrcpy ];
      SEMU_BEZEL_TREE = "${packages.bezel-tree}/share/semu/bezel/shaders";  # the placement and layer contracts run from pinned inputs
      dontConfigure = true;
      buildPhase = ''
        export HOME="$TMPDIR"  # btrcpy writes its cache under $HOME
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
        export HOME="$TMPDIR"  # btrcpy writes its cache under $HOME
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
        export RETROARCH="${packages.retroarch.unwrapped or packages.retroarch}/bin/retroarch"  # the synthetic core needs no core packages
        export CORE="${syntheticCore}/lib/retroarch/cores/synthetic_libretro.so"
        export LIBGL_DRIVERS_PATH="${pkgs.mesa}/lib/dri"
        export __EGL_VENDOR_LIBRARY_DIRS="${pkgs.mesa}/share/glvnd/egl_vendor.d"
        export __GLX_VENDOR_LIBRARY_NAME=mesa
        export LD_LIBRARY_PATH="${pkgs.mesa}/lib"
        xvfb-run --auto-servernum --server-args="-screen 0 1280x800x24" sh tests/integration/retroarch-headless.sh
        touch "$out"
      '';
    };
    launchSystems = pkgs.stdenv.mkDerivation {  # every RetroArch system through semu launch, the synthetic core as each core file
      name = "semu-launch-systems";
      src = lib.fileset.toSource {
        root = repositoryRoot;
        fileset = ../../../tests/integration/launch-systems.sh;
      };
      nativeBuildInputs = [ pkgs.xvfb-run pkgs.socat pkgs.jq pkgs.procps pkgs.mesa pkgs.findutils ];
      dontConfigure = true;
      dontBuild = true;
      installPhase = ''
        export SEMU_CLI="${packages.semu-cli}/bin/semu"
        export RETROARCH="${packages.retroarch.unwrapped or packages.retroarch}/bin/retroarch"  # the synthetic core needs no core packages
        export CORE="${syntheticCore}/lib/retroarch/cores/synthetic_libretro.so"
        export RENDERER="${packages.semu-renderer}/lib"
        export LIBGL_DRIVERS_PATH="${pkgs.mesa}/lib/dri"
        export __EGL_VENDOR_LIBRARY_DIRS="${pkgs.mesa}/share/glvnd/egl_vendor.d"
        export __GLX_VENDOR_LIBRARY_NAME=mesa
        export LD_LIBRARY_PATH="${pkgs.mesa}/lib"
        xvfb-run --auto-servernum --server-args="-screen 0 1280x800x24" sh tests/integration/launch-systems.sh
        touch "$out"
      '';
    };
    # Real cores booting freely licensed test programs (tests/integration/test_roms.json), one derivation
    # for each: they run side by side, each in its sandbox with a network of its own (RetroArch's command
    # port is the same number in all of them), and a core that fails is named and built again alone.
    realCores = let
      manifest = lib.importJSON (repositoryRoot + "/tests/integration/test_roms.json");
      coreFile = core: "${coreFlakes.${core}.packages.${system}.default}/lib/retroarch/cores/${core}_libretro.so";
      romFile = rom: if (rom.archive or null) == "zip"
        then "${pkgs.fetchzip { inherit (rom) url hash; stripRoot = false; }}/${rom.file}"  # the named file inside a release zip
        else pkgs.fetchurl { inherit (rom) url hash; name = rom.file; };
      # The state this program left under the lock before this one, where one is kept (ci/states.sh keeps
      # them). They are saved on x86_64, which is what the machines that hold real states are.
      previousState = rom: let kept = repositoryRoot + "/tests/integration/states/${rom.system}-${rom.core}.state";
        in lib.optionalString (system == "x86_64-linux" && builtins.pathExists kept) "${kept}";
      realCore = rom: pkgs.stdenv.mkDerivation {
        name = "semu-real-core-${rom.system}-${rom.core}";
        src = lib.fileset.toSource { root = repositoryRoot; fileset = ../../../tests/integration/real-cores.sh; };
        nativeBuildInputs = [ pkgs.xvfb-run pkgs.socat pkgs.jq pkgs.imagemagick pkgs.mesa pkgs.findutils ];
        dontConfigure = true;
        dontBuild = true;
        installPhase = ''
          export SEMU_CLI="${packages.semu-cli}/bin/semu"
          export RETROARCH="${packages.retroarch.unwrapped or packages.retroarch}/bin/retroarch"
          export RENDERER="${packages.semu-renderer}/lib"
          export SYSTEM=${rom.system} CORE=${rom.core} ROM="${romFile rom}" COREFILE="${coreFile rom.core}"
          export LIBGL_DRIVERS_PATH="${pkgs.mesa}/lib/dri"
          export __EGL_VENDOR_LIBRARY_DIRS="${pkgs.mesa}/share/glvnd/egl_vendor.d"
          export __GLX_VENDOR_LIBRARY_NAME=mesa
          export LD_LIBRARY_PATH="${pkgs.mesa}/lib"
          mkdir -p "$out"
          export STATE_OUT="$out/state" PREVIOUS_STATE="${previousState rom}"
          xvfb-run --auto-servernum --server-args="-screen 0 1280x800x24" sh tests/integration/real-cores.sh | tee "$out/log"
        '';
      };
    in pkgs.linkFarm "semu-real-cores" (map (rom: { name = "${rom.system}-${rom.core}"; path = realCore rom; }) manifest.roms);
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
    # The emulators build from the committed C of the loader and the RetroArch bridge, so that a new
    # compiler is no input of theirs. Here the compiler makes both afresh, and they must be the same.
    takesCompiler = file: (lib.functionArgs (import file)) ? btrcpy;
    generatedC = assert lib.assertMsg (!takesCompiler ../../../src/renderer/loader.nix && !takesCompiler ../../../config/emulators/retroarch/package.nix && !takesCompiler ../../../config/emulators/retroarch/darwin.nix)
      "the loader and RetroArch must not take the compiler: a new compiler would rebuild every emulator";
      pkgs.runCommand "semu-generated-c" { } ''
      current() {
        ${pkgs.diffutils}/bin/diff -u "$1" "$2" && return
        echo "$3 is not what the compiler makes of its source: run ci/generated.sh and commit what it changes" >&2
        exit 1
      }
      current ${../../../src/renderer/loader/generated/semu_renderer_loader.c} ${renderer.packages.${system}.generated}/semu_renderer_loader.c src/renderer/loader/generated/semu_renderer_loader.c
      current ${../../../src/renderer/retroarch/generated/semu_retroarch.c} ${renderer.packages.${system}.generated}/semu_retroarch.c src/renderer/retroarch/generated/semu_retroarch.c
      touch "$out"
    '';
  in { inherit contracts; generated-c = generatedC; } // lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
    inherit installer;
    platform-matrix = platformMatrix;
    launch-systems = launchSystems;
    real-cores = realCores;
    synthetic-core = syntheticCore;
    retroarch-headless = retroarchHeadless;
    semu = packages.semu;
    retroarch = packages.retroarch;
    es-de = packages.es-de;
  })
