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
    ../../../build/packaging/nix/patches/pcsx2/001-semu-render-hook.patch
  ];
  semuPatchAssertions = ''
    grep -F 'semuRenderHookProof("pcsx2");' pcsx2/GS/Renderers/Common/GSRenderer.cpp
    grep -F 'semuRenderHookComposeFrame(' pcsx2/GS/Renderers/Common/GSRenderer.cpp
    grep -F 'semuRenderHookPresentFrame(' pcsx2/GS/Renderers/Common/GSRenderer.cpp
    grep -F 'semuRenderHookPresentContent(' pcsx2/GS/Renderers/Common/GSRenderer.cpp
    grep -F 'SEMU_RENDER_SHADERS' pcsx2/GS/Renderers/Common/GSRenderer.cpp
    grep -F 'SEMU_RENDER_BEZELS' pcsx2/GS/Renderers/Common/GSRenderer.cpp
    grep -F 'SEMU_RENDER_HOOK_CONFIG' pcsx2/GS/Renderers/Common/GSRenderer.cpp
    grep -F 'SEMU_RENDER_HOOK_PROOF' pcsx2/GS/Renderers/Common/GSRenderer.cpp
    grep -F 'semu-render-hook:%s:game_framebuffer:config=%s' pcsx2/GS/Renderers/Common/GSRenderer.cpp
  '';
  sourceHook = {
    status = "source-package-ready";
    requiresSourceBuild = true;
    productionPatchable = true;
    sourcePackagePath = "pkgs.pcsx2";
    sourcePlatforms = [ "x86_64-linux" ];
    binaryPackagePaths = [ "../../../build/packaging/nix/pcsx2-mac.nix" ];
    binaryPackageStatus = "no-source-hook";
    contractProof = {
      marker = "semu-render-hook:pcsx2:game_framebuffer";
      configEnv = "SEMU_RENDER_HOOK_CONFIG";
      proofEnv = "SEMU_RENDER_HOOK_PROOF";
      usesNativeShaderMechanism = false;
      patchFiles = [
        "../../../build/packaging/nix/patches/pcsx2/001-semu-render-hook.patch"
      ];
      patchedFiles = [
        "pcsx2/GS/Renderers/Common/GSRenderer.cpp"
      ];
    };
    remainingGap = null;
  };
  darwinSourceHook = sourceHook // {
    status = "binary-package-no-source-hook";
    productionPatchable = false;
    sourcePackagePath = null;
    remainingGap = "macOS uses a prebuilt PCSX2 release; add a Darwin source package before enabling source hooks there.";
  };
in
if pkgs.stdenv.hostPlatform.isDarwin then
  semuPackage.withSourceHookMetadata {
    id = "pcsx2";
    package = pkgs.callPackage ../../../build/packaging/nix/pcsx2-mac.nix {};
    sourceHook = darwinSourceHook;
  }
else
  semuPackage.mkSourceOverride {
    id = "pcsx2";
    base = pkgs.pcsx2;
    inherit source includeBasePatches;
    patches = semuPatches ++ patches;
    postPatchAssertions = semuPatchAssertions + "\n" + postPatchAssertions;
    extraAttrs = semuPackage.sourceHookExtraAttrs {
      id = "pcsx2";
      inherit sourceHook extraAttrs;
    };
  }
