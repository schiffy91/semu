{ pkgs
, semuPackage ? pkgs.callPackage ../../../build/packaging/nix/lib/source-package.nix {}
, source ? null
, patches ? []
, includeBasePatches ? source == null
, postPatchAssertions ? ""
, extraAttrs ? {}
}:

let
  semuPatches = [
    ../../../build/packaging/nix/patches/retroarch/001-semu-render-hook.patch
  ];
  semuPatchAssertions = ''
    grep -F 'semuRenderHookProof("retroarch");' gfx/video_driver.c
    grep -F 'semuRenderHookComposeFrame(' gfx/video_driver.c
    grep -F 'semuRenderHookProfileForSystem' gfx/video_driver.c
    grep -F 'SEMU_RENDER_SHADERS' gfx/video_driver.c
    grep -F 'SEMU_RENDER_BEZELS' gfx/video_driver.c
    grep -F 'SEMU_RENDER_HOOK_CONFIG' gfx/video_driver.c
    grep -F 'SEMU_RENDER_HOOK_PROOF' gfx/video_driver.c
    grep -F 'SEMU_RENDER_PROFILE_DMG_LCD' gfx/video_driver.c
    grep -F 'SEMU_RENDER_PROFILE_GBA_LCD' gfx/video_driver.c
    grep -F 'SEMU_RENDER_PROFILE_CRT' gfx/video_driver.c
    grep -F 'semu-render-hook:%s:game_framebuffer:config=%s' gfx/video_driver.c
  '';
  sourceHook = {
    status = "source-package-ready";
    requiresSourceBuild = true;
    productionPatchable = true;
    sourcePackagePath = "pkgs.retroarch.unwrapped";
    sourcePlatforms = [ "x86_64-linux" "aarch64-darwin" "x86_64-darwin" ];
    binaryPackagePaths = [ "../../../build/packaging/nix/retroarch-mac.nix" ];
    binaryPackageStatus = "no-source-hook";
    contractProof = {
      marker = "semu-render-hook:retroarch:game_framebuffer";
      configEnv = "SEMU_RENDER_HOOK_CONFIG";
      proofEnv = "SEMU_RENDER_HOOK_PROOF";
      usesNativeShaderMechanism = false;
      patchFiles = [
        "../../../build/packaging/nix/patches/retroarch/001-semu-render-hook.patch"
      ];
      patchedFiles = [
        "gfx/video_driver.c"
      ];
    };
    remainingGap = null;
  };
  darwinSourceHook = sourceHook // {
    status = "binary-package-no-source-hook";
    productionPatchable = false;
    sourcePackagePath = null;
    remainingGap = "macOS uses a prebuilt RetroArch release; add a Darwin source package before enabling source hooks there.";
  };
  patchedRetroarch =
    semuPackage.mkSourceOverride {
      id = "retroarch";
      base = pkgs.retroarch.unwrapped;
      inherit source includeBasePatches;
      patches = semuPatches ++ patches;
      postPatchAssertions = semuPatchAssertions + "\n" + postPatchAssertions;
      extraAttrs = semuPackage.sourceHookExtraAttrs {
        id = "retroarch";
        inherit sourceHook extraAttrs;
      };
    };
  cores = [
    pkgs.libretro.gambatte
    pkgs.libretro.mgba
    pkgs.libretro.genesis-plus-gx
    pkgs.libretro.snes9x
    pkgs.libretro.mesen
    pkgs.libretro.mupen64plus
    pkgs.libretro.desmume
    pkgs.libretro.beetle-psx
    pkgs.libretro.beetle-psx-hw
    pkgs.libretro.ppsspp
    pkgs.libretro.flycast
    pkgs.libretro.dolphin
  ];
  settingsPath = pkgs.writeText "semu-retroarch.cfg" (
    pkgs.lib.concatStringsSep "\n" (pkgs.lib.mapAttrsToList (n: v: "${n} = \"${v}\"") {
      assets_directory = "${pkgs.retroarch-assets}/share/retroarch/assets";
      joypad_autoconfig_dir = "${pkgs.retroarch-joypad-autoconfig}/share/libretro/autoconfig";
      libretro_info_path = "${pkgs.libretro-core-info}/share/retroarch/cores";
    })
  );
  wrapperArgs = pkgs.lib.strings.escapeShellArgs [
    "--set"
    "SEMU_RETROARCH_CORE_DIR"
    "${placeholder "out"}/lib/retroarch/cores"
    "--add-flags"
    "--appendconfig=${settingsPath}"
  ];
in
if pkgs.stdenv.hostPlatform.isDarwin then
  semuPackage.withSourceHookMetadata {
    id = "retroarch";
    package = pkgs.callPackage ../../../build/packaging/nix/retroarch-mac.nix {};
    sourceHook = darwinSourceHook;
  }
else
  pkgs.symlinkJoin {
    pname = "retroarch-with-cores";
    version = pkgs.lib.getVersion patchedRetroarch;
    paths = [ patchedRetroarch ] ++ cores;
    nativeBuildInputs = [ pkgs.makeBinaryWrapper ];
    passthru = {
      inherit cores;
      unwrapped = patchedRetroarch;
      semu = {
        id = "retroarch";
        inherit sourceHook;
      };
      tests = patchedRetroarch.passthru.tests or {};
    };
    postBuild = ''
      find $out/bin -name 'retroarch-*' -type l -delete
      wrapProgram $out/bin/retroarch ${wrapperArgs}
    '';
    meta = patchedRetroarch.meta // {
      longDescription = ''
        RetroArch is the reference frontend for the libretro API.
        This Semu package wraps the patched frontend with the libretro cores required by served systems.
      '';
    };
  }
