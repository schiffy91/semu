# Flake checks: every package evaluates, the contract tests pass, and real RetroArch runs headless.
{ self, forAllSystems, mkPkgs, ... }:

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
        fileset = lib.fileset.unions [ ../../../src ../../../tests/contracts ../../../config ];
      };
      nativeBuildInputs = [ packages.btrcpy ];
      dontConfigure = true;
      buildPhase = ''
        btrcpy tests/contracts/main.btrc -o contracts.c --strict-imports --no-cache --no-stdlib
        $CC contracts.c -std=c11 -O1 -o contracts -lm
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
        $CC core.c -std=gnu11 -O2 -shared -fPIC -I${pkgs.retroarch-bare.src}/libretro-common/include -o synthetic_libretro.so
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
      nativeBuildInputs = [ pkgs.xvfb-run pkgs.python3 pkgs.imagemagick pkgs.mesa pkgs.findutils pkgs.gawk ];
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
  in {
    inherit contracts installer;
    synthetic-core = syntheticCore;
    retroarch-headless = retroarchHeadless;
    semu = packages.semu;
    retroarch = packages.retroarch;
    es-de = packages.es-de;
  })
