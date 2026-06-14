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
    ../../../build/packaging/nix/patches/azahar/001-semu-render-hook.patch
  ];
  semuPatchAssertions = ''
    grep -F 'semuRenderHookComposeFrame(framebuffer_width, framebuffer_height)' src/citra_qt/bootmanager.cpp
    grep -F 'semuRenderHookProof("azahar");' src/citra_qt/bootmanager.cpp
    grep -F 'semuRenderHookFragmentShaderBody' src/citra_qt/bootmanager.cpp
    grep -F 'glCopyTexImage2D' src/citra_qt/bootmanager.cpp
    grep -F 'dual_vertical' src/citra_qt/bootmanager.cpp
    grep -F 'SEMU_RENDER_SHADERS' src/citra_qt/bootmanager.cpp
    grep -F 'SEMU_RENDER_BEZELS' src/citra_qt/bootmanager.cpp
    grep -F 'SEMU_SYSTEM' src/citra_qt/bootmanager.cpp
    grep -F 'SEMU_RENDER_HOOK_CONFIG' src/citra_qt/bootmanager.cpp
    grep -F 'SEMU_RENDER_HOOK_PROOF' src/citra_qt/bootmanager.cpp
    grep -F 'semu-render-hook:%s:game_framebuffer:config=%s' src/citra_qt/bootmanager.cpp
  '';
  sourceHook = {
    status = "source-package-ready";
    requiresSourceBuild = true;
    productionPatchable = true;
    sourcePackagePath = "pkgs.azahar";
    sourcePlatforms = [ "x86_64-linux" ];
    binaryPackagePaths = [ "../../../build/packaging/nix/azahar-mac.nix" ];
    binaryPackageStatus = "no-source-hook";
    contractProof = {
      marker = "semu-render-hook:azahar:game_framebuffer";
      configEnv = "SEMU_RENDER_HOOK_CONFIG";
      proofEnv = "SEMU_RENDER_HOOK_PROOF";
      usesNativeShaderMechanism = false;
      patchFiles = [
        "../../../build/packaging/nix/patches/azahar/001-semu-render-hook.patch"
      ];
      patchedFiles = [
        "src/citra_qt/bootmanager.cpp"
      ];
    };
    remainingGap = null;
  };
  darwinSourceHook = sourceHook // {
    status = "binary-package-no-source-hook";
    productionPatchable = false;
    sourcePackagePath = null;
    remainingGap = "macOS uses a prebuilt Azahar release; add a Darwin source package before enabling source hooks there.";
  };
in
if pkgs.stdenv.hostPlatform.isDarwin then
  semuPackage.withSourceHookMetadata {
    id = "azahar";
    package = pkgs.callPackage ../../../build/packaging/nix/azahar-mac.nix {};
    sourceHook = darwinSourceHook;
  }
else
  semuPackage.mkSourceOverride {
    id = "azahar";
    base = pkgs.azahar;
    inherit source includeBasePatches;
    patches = semuPatches ++ patches;
    postPatchAssertions = semuPatchAssertions + "\n" + postPatchAssertions;
    extraAttrs = semuPackage.sourceHookExtraAttrs {
      id = "azahar";
      inherit sourceHook extraAttrs;
    };
  }
