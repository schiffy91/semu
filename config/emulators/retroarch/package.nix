# RetroArch built from the nixpkgs recipe with Semu's render hook: gl3 calls the
# shared renderer after the game draw and before present. Cores come from
# nixpkgs and are selected from the linux system bindings.
{ lib, pkgs, callPackage, retroarch-bare, libretro, makeBinaryWrapper, writeText, symlinkJoin, btrcpy, semuRenderer, retroarch-assets, retroarch-joypad-autoconfig, libretro-core-info }:

let
  repositoryRoot = ../../..;
  systemsDir = repositoryRoot + "/config/systems";
  coreManifest = lib.importJSON ./cores.json;
  systemIds = lib.attrNames (lib.filterAttrs (_: type: type == "directory") (builtins.readDir systemsDir));
  systemContracts = map (id: lib.importJSON (systemsDir + "/${id}/system.json")) systemIds;
  linuxBinding = entry: (entry.emulator or "") == "retroarch" && entry ? core && lib.elem "linux" (entry.platforms or [ "linux" ]);
  selectedCores = lib.unique (lib.concatMap (system: map (entry: entry.core) (lib.filter linuxBinding (system.emulators or [ ]))) systemContracts);
  corePackage = core:
    let
      implementation = coreManifest.implementations.${core}.linux or (throw "retroarch: core '${core}' has no linux implementation in cores.json");
      attribute = implementation.package_attribute;
    in
    assert lib.assertMsg (implementation.kind == "nixpkgs_libretro") "retroarch: core '${core}' is not a nixpkgs libretro core";
    libretro.${attribute} or (throw "retroarch: nixpkgs libretro has no '${attribute}' for core '${core}'");
  bridgeSource = lib.fileset.toSource {
    root = repositoryRoot;
    fileset = repositoryRoot + "/src/renderer/retroarch";
  };
  hooked = retroarch-bare.overrideAttrs (previous: {
    pname = "retroarch-semu";
    patches = (previous.patches or [ ]) ++ [ ./retroarch.patch ./retroarch_commands.patch ./retroarch_get_status_null_safety.patch ];
    nativeBuildInputs = (previous.nativeBuildInputs or [ ]) ++ [ btrcpy ];
    buildInputs = (previous.buildInputs or [ ]) ++ [ semuRenderer ];
    postPatch = (previous.postPatch or "") + ''
      btrcpy ${bridgeSource}/src/renderer/retroarch/runtime_bridge.btrc -o gfx/semu_retroarch.c \
        --strict-imports --no-cache --no-stdlib --no-dce
      test -s gfx/semu_retroarch.c
    '';
    env = (previous.env or { }) // {
      NIX_CFLAGS_COMPILE = (previous.env.NIX_CFLAGS_COMPILE or "") + " -DHAVE_SEMU_RENDERER -I${semuRenderer}/include";
      NIX_LDFLAGS = (previous.env.NIX_LDFLAGS or "") + " -L${semuRenderer}/lib -rpath ${semuRenderer}/lib -lsemurenderer";
    };
    postInstall = (previous.postInstall or "") + ''
      grep -Fq 'libsemurenderer.so' <<<"$(readelf -d "$out/bin/retroarch")"
    '';
  });
  wrapper = import (pkgs.path + "/pkgs/by-name/re/retroarch-bare/wrapper.nix") {
    inherit lib libretro makeBinaryWrapper writeText symlinkJoin;
    retroarch-bare = hooked;
    cores = map corePackage selectedCores;
    settings = {
      assets_directory = "${retroarch-assets}/share/retroarch/assets";
      joypad_autoconfig_dir = "${retroarch-joypad-autoconfig}/share/libretro/autoconfig";
      libretro_info_path = "${libretro-core-info}/share/retroarch/cores";
    };
  };
in
wrapper.overrideAttrs (previous: {
  passthru = (previous.passthru or { }) // { semuSelectedCores = selectedCores; unwrapped = hooked; };
})
