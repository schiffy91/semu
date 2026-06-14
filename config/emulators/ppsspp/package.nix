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
    ../../../build/packaging/nix/patches/ppsspp/001-semu-render-hook.patch
  ];
  semuPatchAssertions = ''
    grep -F 'semuRenderHookProof("ppsspp");' GPU/Common/PresentationCommon.cpp
    grep -F 'semuRenderHookComposeFrame(' GPU/Common/PresentationCommon.cpp
    grep -F 'semuRenderHookCreateColorPipeline(draw_)' GPU/Common/PresentationCommon.cpp
    grep -F 'SEMU_RENDER_SHADERS' GPU/Common/PresentationCommon.cpp
    grep -F 'SEMU_RENDER_BEZELS' GPU/Common/PresentationCommon.cpp
    grep -F 'SEMU_RENDER_HOOK_CONFIG' GPU/Common/PresentationCommon.cpp
    grep -F 'SEMU_RENDER_HOOK_PROOF' GPU/Common/PresentationCommon.cpp
    grep -F 'SemuRenderHookProfile::PSP_LCD' GPU/Common/PresentationCommon.cpp
    grep -F 'semu-render-hook:%s:game_framebuffer:config=%s' GPU/Common/PresentationCommon.cpp
  '';
  sourceHook = {
    status = "source-package-ready";
    requiresSourceBuild = true;
    productionPatchable = true;
    sourcePackagePath = "pkgs.ppsspp-sdl-wayland";
    sourcePlatforms = [ "x86_64-linux" ];
    binaryPackagePaths = [];
    contractProof = {
      marker = "semu-render-hook:ppsspp:game_framebuffer";
      configEnv = "SEMU_RENDER_HOOK_CONFIG";
      proofEnv = "SEMU_RENDER_HOOK_PROOF";
      usesNativeShaderMechanism = false;
      patchFiles = [
        "../../../build/packaging/nix/patches/ppsspp/001-semu-render-hook.patch"
      ];
      patchedFiles = [
        "GPU/Common/PresentationCommon.h"
        "GPU/Common/PresentationCommon.cpp"
      ];
    };
    remainingGap = null;
  };
in
semuPackage.mkSourceOverride {
  id = "ppsspp";
  base = pkgs.ppsspp-sdl-wayland;
  inherit source includeBasePatches;
  patches = semuPatches ++ patches;
  postPatchAssertions = semuPatchAssertions + "\n" + postPatchAssertions;
  extraAttrs = semuPackage.sourceHookExtraAttrs {
    id = "ppsspp";
    inherit sourceHook extraAttrs;
  };
}
