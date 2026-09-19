# RetroArch built from the nixpkgs recipe with Semu's render hook: gl3 calls the
# shared renderer after the game draw and before present. Cores come from
# nixpkgs and are selected from the linux system bindings.
{ lib, pkgs, callPackage, fetchFromGitHub, fetchFromForgejo, retroarch-bare, libretro, makeBinaryWrapper, writeText, symlinkJoin, btrcpy, semuRenderer, retroarch-assets, retroarch-joypad-autoconfig, libretro-core-info }:

let
  repositoryRoot = ../../..;
  systemsDir = repositoryRoot + "/config/systems";
  coreManifest = lib.importJSON ./cores.json;
  packageContract = lib.importJSON ./package.json;
  pinned = import ../../../packaging/nix/pinned_source.nix { inherit lib fetchFromGitHub fetchFromForgejo; };
  systemIds = lib.attrNames (lib.filterAttrs (_: type: type == "directory") (builtins.readDir systemsDir));
  systemContracts = map (id: lib.importJSON (systemsDir + "/${id}/system.json")) systemIds;
  linuxBinding = entry: (entry.emulator or "") == "retroarch" && entry ? core && lib.elem "linux" (entry.platforms or [ "linux" ]);
  selectedCores = lib.unique (lib.concatMap (system: map (entry: entry.core) (lib.filter linuxBinding (system.emulators or [ ]))) systemContracts);
  corePackage = core:
    let
      implementation = coreManifest.implementations.${core}.linux or (throw "retroarch: core '${core}' has no linux implementation in cores.json");
      attribute = implementation.package_attribute;
    in
    assert lib.assertMsg (implementation.kind == "pinned_source") "retroarch: core '${core}' must be a pinned source build";
    (libretro.${attribute} or (throw "retroarch: nixpkgs libretro has no recipe '${attribute}' for core '${core}'")).overrideAttrs (previous: {
      src = pinned implementation.source;  # Semu's pin, nixpkgs' build wiring
      allowSubstitutes = false;  # never a cache binary
    });
  bridgeSource = lib.fileset.toSource {
    root = repositoryRoot;
    fileset = repositoryRoot + "/src/renderer/retroarch";
  };
  hooked = retroarch-bare.overrideAttrs (previous: {
    pname = "retroarch-semu";
    version = packageContract.version;
    src = pinned packageContract.source;
    allowSubstitutes = false;  # never a cache binary
    patches = (previous.patches or [ ]) ++ [ ./retroarch.patch ./retroarch_commands.patch ./retroarch_get_status_null_safety.patch ];
    nativeBuildInputs = (previous.nativeBuildInputs or [ ]) ++ [ btrcpy ];
    buildInputs = (previous.buildInputs or [ ]) ++ [ semuRenderer ];
    postPatch = (previous.postPatch or "") + ''
      btrcpy ${bridgeSource}/src/renderer/retroarch/runtime_bridge.btrc -o gfx/semu_retroarch.c \
        --strict-imports --no-cache --no-stdlib --no-dce
      test -s gfx/semu_retroarch.c
      # Software cores get a 3.2 core context by default; the renderer and librashader need 3.3 / GLSL 330.
      sed -i '/major = 3;/{n;s/minor = 2;/minor = 3;/}' gfx/drivers/gl3.c
      grep -Fq 'minor = 3;' gfx/drivers/gl3.c
      # Cores with version-gated loaders (Citra's glad) see only 3.3 entry points in a 3.3 context; give core-profile requests 4.6.
      sed -i '/gl_query_core_context_set(hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE);/a\      if (hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE \&\& major == 3) { major = 4; minor = 6; }' gfx/drivers/gl3.c
      grep -Fq 'major = 4; minor = 6;' gfx/drivers/gl3.c
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
