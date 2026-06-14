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
    ../../../build/packaging/nix/patches/melonds/001-semu-render-hook.patch
  ];
  semuPatchAssertions = ''
    grep -F 'semuRenderHookComposeFrame(w, h)' src/frontend/qt_sdl/Screen.cpp
    grep -F 'semuRenderHookProof("melonds");' src/frontend/qt_sdl/Screen.cpp
    grep -F 'semuRenderHookFragmentShaderBody' src/frontend/qt_sdl/Screen.cpp
    grep -F 'glCopyTexImage2D' src/frontend/qt_sdl/Screen.cpp
    grep -F 'dual_vertical' src/frontend/qt_sdl/Screen.cpp
    grep -F 'SEMU_RENDER_SHADERS' src/frontend/qt_sdl/Screen.cpp
    grep -F 'SEMU_RENDER_BEZELS' src/frontend/qt_sdl/Screen.cpp
    grep -F 'SEMU_SYSTEM' src/frontend/qt_sdl/Screen.cpp
    grep -F 'SEMU_RENDER_HOOK_CONFIG' src/frontend/qt_sdl/Screen.cpp
    grep -F 'SEMU_RENDER_HOOK_PROOF' src/frontend/qt_sdl/Screen.cpp
    grep -F 'semu-render-hook:%s:game_framebuffer:config=%s' src/frontend/qt_sdl/Screen.cpp
  '';
  sourceHook = {
    status = "source-package-ready";
    requiresSourceBuild = true;
    productionPatchable = true;
    sourcePackagePath = "pkgs.melonds";
    sourcePlatforms = [ "x86_64-linux" ];
    binaryPackagePaths = [];
    contractProof = {
      marker = "semu-render-hook:melonds:game_framebuffer";
      configEnv = "SEMU_RENDER_HOOK_CONFIG";
      proofEnv = "SEMU_RENDER_HOOK_PROOF";
      usesNativeShaderMechanism = false;
      patchFiles = [
        "../../../build/packaging/nix/patches/melonds/001-semu-render-hook.patch"
      ];
      patchedFiles = [
        "src/frontend/qt_sdl/Screen.cpp"
      ];
    };
    remainingGap = null;
  };
in
semuPackage.mkSourceOverride {
  id = "melonds";
  base = pkgs.melonds;
  inherit source includeBasePatches;
  patches = semuPatches ++ patches;
  postPatchAssertions = semuPatchAssertions + "\n" + postPatchAssertions;
  extraAttrs = semuPackage.sourceHookExtraAttrs {
    id = "melonds";
    inherit sourceHook extraAttrs;
  };
}
