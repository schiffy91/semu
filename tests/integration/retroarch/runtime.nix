let
  repository = ../../..;
  rootFlake = builtins.getFlake (toString repository);
  system = builtins.currentSystem;
  pkgs = import rootFlake.inputs.nixpkgs { inherit system; };
  btrcpy = rootFlake.packages.${system}.btrcpy;
  retroarch = rootFlake.packages.${system}.retroarch-runtime;
  contract = builtins.fromJSON (
    builtins.readFile (repository + "/config/emulators/retroarch/package.json")
  );
  retroarchSource = pkgs.fetchFromGitHub {
    inherit (contract.source) owner repo;
    rev = contract.source.revision;
    hash = contract.source.sha256;
  };
  testSource = pkgs.lib.fileset.toSource {
    root = repository;
    fileset = pkgs.lib.fileset.unions [
      ./control.btrc
      ./glass_fixture.btrc
      ./synthetic_core.btrc
      ./verify.btrc
      ../../../src/generators/input/action_abi.btrc
      ../../../src/lib/owned_paths.btrc
    ];
  };
  strictBtrc =
    {
      name,
      source,
      shared ? false,
      deadCodeElimination ? false,
      includeDirectories ? [ ],
    }:
    pkgs.stdenv.mkDerivation {
      inherit name;
      dontUnpack = true;
      nativeBuildInputs = [ btrcpy ];
      buildPhase = ''
        runHook preBuild
        btrcpy ${source} -o program.c \
          --strict-imports --no-cache --no-stdlib \
          ${pkgs.lib.optionalString (!deadCodeElimination) "--no-dce"}
        cc program.c -std=gnu11 -O2 -Wall -Wextra -Werror \
          ${pkgs.lib.optionalString shared "-shared -fPIC"} \
          ${pkgs.lib.concatMapStringsSep " " (path: "-I${path}") includeDirectories} \
          -o program ${pkgs.lib.optionalString (!shared) "-lm"}
        runHook postBuild
      '';
      installPhase = ''
        runHook preInstall
        mkdir -p "$out/bin"
        cp program "$out/bin/${name}"
        runHook postInstall
      '';
    };
  syntheticCore = strictBtrc {
    name = "semu_synthetic_libretro.so";
    source = testSource + "/tests/integration/retroarch/synthetic_core.btrc";
    shared = true;
    includeDirectories = [
      (retroarchSource + "/libretro-common/include")
    ];
  };
  control = strictBtrc {
    name = "semu-retroarch-control";
    source = testSource + "/tests/integration/retroarch/control.btrc";
  };
  glassFixture = strictBtrc {
    name = "semu-retroarch-glass-fixture";
    source = testSource + "/tests/integration/retroarch/glass_fixture.btrc";
    deadCodeElimination = true;
  };
  verifier = strictBtrc {
    name = "semu-retroarch-verify";
    source = testSource + "/tests/integration/retroarch/verify.btrc";
    deadCodeElimination = true;
  };
in
pkgs.symlinkJoin {
  name = "semu-retroarch-real-integration";
  paths = [
    retroarch
    syntheticCore
    control
    glassFixture
    verifier
    pkgs.bash
    pkgs.coreutils
    pkgs.gnugrep
    pkgs.gnused
    pkgs.gawk
    pkgs.glibc.bin
    pkgs.procps
    pkgs.netcat-openbsd
    pkgs.xdotool
    pkgs.xinput
    pkgs.xauth
    pkgs.xdpyinfo
    pkgs.xprop
    pkgs.xset
    pkgs.xwd
    pkgs.xwininfo
    pkgs."xorg-server"
    pkgs."xf86-video-dummy"
    pkgs."xf86-input-void"
    pkgs.imagemagick
    pkgs.mesa
    pkgs.mesa-demos
  ];
  postBuild = ''
    mkdir -p "$out/lib/libretro"
    mv "$out/bin/semu_synthetic_libretro.so" \
      "$out/lib/libretro/semu_synthetic_libretro.so"
    test -x "$out/bin/retroarch"
    test -L "$out/bin/retroarch-semu"
    test -x "$out/bin/semu-retroarch-control"
    test -x "$out/bin/semu-retroarch-glass-fixture"
    test -x "$out/bin/semu-retroarch-verify"
    test -s "$out/lib/libretro/semu_synthetic_libretro.so"
  '';
  passthru = {
    inherit retroarch syntheticCore;
  };
}
