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
    ../../../build/packaging/nix/patches/dolphin/001-semu-render-hook.patch
  ];
  semuPatchAssertions = ''
    grep -F 'semuRenderHookProof("dolphin");' Source/Core/VideoCommon/Present.cpp
    grep -F 'semuRenderHookComposeFrame(' Source/Core/VideoCommon/Present.cpp
    grep -F 'semuRenderHookRenderContent(' Source/Core/VideoCommon/Present.cpp
    grep -F 'semuRenderHookInsetContentRect' Source/Core/VideoCommon/Present.cpp
    grep -F 'SEMU_RENDER_SHADERS' Source/Core/VideoCommon/Present.cpp
    grep -F 'SEMU_RENDER_BEZELS' Source/Core/VideoCommon/Present.cpp
    grep -F 'SEMU_RENDER_HOOK_CONFIG' Source/Core/VideoCommon/Present.cpp
    grep -F 'SEMU_RENDER_HOOK_PROOF' Source/Core/VideoCommon/Present.cpp
    grep -F 'semu-render-hook:%s:game_framebuffer:config=%s' Source/Core/VideoCommon/Present.cpp
  '';
  sourceHook = {
    status = "source-package-ready";
    requiresSourceBuild = true;
    productionPatchable = true;
    sourcePackagePath = "pkgs.dolphin-emu";
    sourcePlatforms = [ "x86_64-linux" "aarch64-darwin" "x86_64-darwin" ];
    binaryPackagePaths = [];
    contractProof = {
      marker = "semu-render-hook:dolphin:game_framebuffer";
      configEnv = "SEMU_RENDER_HOOK_CONFIG";
      proofEnv = "SEMU_RENDER_HOOK_PROOF";
      usesNativeShaderMechanism = false;
      patchFiles = [
        "../../../build/packaging/nix/patches/dolphin/001-semu-render-hook.patch"
      ];
      patchedFiles = [
        "Source/Core/VideoCommon/Present.cpp"
      ];
    };
    remainingGap = null;
  };
in
semuPackage.mkSourceOverride {
  id = "dolphin";
  base = pkgs.dolphin-emu;
  inherit source includeBasePatches;
  patches = semuPatches ++ patches;
  postPatchAssertions = semuPatchAssertions + "\n" + postPatchAssertions;
  extraAttrs = semuPackage.sourceHookExtraAttrs {
    id = "dolphin";
    inherit sourceHook extraAttrs;
  };
}
