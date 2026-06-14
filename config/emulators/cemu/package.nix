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
    ../../../build/packaging/nix/patches/cemu/001-semu-render-hook.patch
  ];
  semuPatchAssertions = ''
    grep -F 'semuRenderHookProof("cemu");' src/Cafe/HW/Latte/Core/LatteRenderTarget.cpp
    grep -F 'semuRenderHookComposeFrame(' src/Cafe/HW/Latte/Core/LatteRenderTarget.cpp
    grep -F 'semuRenderHookShader(renderUpsideDown)' src/Cafe/HW/Latte/Core/LatteRenderTarget.cpp
    grep -F 'SEMU_RENDER_SHADERS' src/Cafe/HW/Latte/Core/LatteRenderTarget.cpp
    grep -F 'SEMU_RENDER_BEZELS' src/Cafe/HW/Latte/Core/LatteRenderTarget.cpp
    grep -F 'SEMU_RENDER_HOOK_CONFIG' src/Cafe/HW/Latte/Core/LatteRenderTarget.cpp
    grep -F 'SEMU_RENDER_HOOK_PROOF' src/Cafe/HW/Latte/Core/LatteRenderTarget.cpp
    grep -F 'semu-render-hook:%s:game_framebuffer:config=%s' src/Cafe/HW/Latte/Core/LatteRenderTarget.cpp
  '';
  sourceHook = {
    status = "source-package-ready";
    requiresSourceBuild = true;
    productionPatchable = true;
    sourcePackagePath = "pkgs.cemu";
    sourcePlatforms = [ "x86_64-linux" ];
    binaryPackagePaths = [ "../../../build/packaging/nix/cemu-mac.nix" ];
    binaryPackageStatus = "no-source-hook";
    contractProof = {
      marker = "semu-render-hook:cemu:game_framebuffer";
      configEnv = "SEMU_RENDER_HOOK_CONFIG";
      proofEnv = "SEMU_RENDER_HOOK_PROOF";
      usesNativeShaderMechanism = false;
      patchFiles = [
        "../../../build/packaging/nix/patches/cemu/001-semu-render-hook.patch"
      ];
      patchedFiles = [
        "src/Cafe/HW/Latte/Core/LatteRenderTarget.cpp"
      ];
    };
    remainingGap = null;
  };
  package =
    if pkgs.stdenv.hostPlatform.isDarwin then
      pkgs.callPackage ../../../build/packaging/nix/cemu-mac.nix {}
    else
      semuPackage.mkSourceOverride {
        id = "cemu";
        base = pkgs.cemu;
        inherit source includeBasePatches;
        patches = semuPatches ++ patches;
        postPatchAssertions = semuPatchAssertions + "\n" + postPatchAssertions;
        extraAttrs = semuPackage.sourceHookExtraAttrs {
          id = "cemu";
          inherit sourceHook extraAttrs;
        };
      };
in
semuPackage.withSourceHookMetadata {
  id = "cemu";
  inherit package sourceHook;
}
