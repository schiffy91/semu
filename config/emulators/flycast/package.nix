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
    ../../../build/packaging/nix/patches/flycast/001-semu-render-hook.patch
  ];
  semuPatchAssertions = ''
    grep -F 'semuRenderHookProof("flycast");' core/rend/gles/gldraw.cpp
    grep -F 'semuRenderHookComposeFrame(' core/rend/gles/gldraw.cpp
    grep -F 'semuRenderHookCopyTexture()' core/rend/gles/gldraw.cpp
    grep -F 'semuRenderHookDrawCrtMask(content);' core/rend/gles/gldraw.cpp
    grep -F 'SEMU_RENDER_SHADERS' core/rend/gles/gldraw.cpp
    grep -F 'SEMU_RENDER_BEZELS' core/rend/gles/gldraw.cpp
    grep -F 'SEMU_RENDER_HOOK_CONFIG' core/rend/gles/gldraw.cpp
    grep -F 'SEMU_RENDER_HOOK_PROOF' core/rend/gles/gldraw.cpp
    grep -F 'SemuRenderHookProfile::DreamcastCrt' core/rend/gles/gldraw.cpp
    grep -F 'semu-render-hook:%s:game_framebuffer:config=%s' core/rend/gles/gldraw.cpp
  '';
  sourceHook = {
    status = "source-package-ready";
    requiresSourceBuild = true;
    productionPatchable = true;
    sourcePackagePath = "pkgs.flycast";
    sourcePlatforms = [ "x86_64-linux" ];
    binaryPackagePaths = [];
    contractProof = {
      marker = "semu-render-hook:flycast:game_framebuffer";
      configEnv = "SEMU_RENDER_HOOK_CONFIG";
      proofEnv = "SEMU_RENDER_HOOK_PROOF";
      usesNativeShaderMechanism = false;
      patchFiles = [
        "../../../build/packaging/nix/patches/flycast/001-semu-render-hook.patch"
      ];
      patchedFiles = [
        "core/rend/gles/gldraw.cpp"
      ];
    };
    remainingGap = null;
  };
in
semuPackage.mkSourceOverride {
  id = "flycast";
  base = pkgs.flycast;
  inherit source includeBasePatches;
  patches = semuPatches ++ patches;
  postPatchAssertions = semuPatchAssertions + "\n" + postPatchAssertions;
  extraAttrs = semuPackage.sourceHookExtraAttrs {
    id = "flycast";
    inherit sourceHook extraAttrs;
  };
}
