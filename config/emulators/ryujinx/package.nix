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
    ../../../build/packaging/nix/patches/ryujinx/001-semu-render-hook.patch
  ];
  semuPatchAssertions = ''
    grep -F 'SemuRenderHookProof("ryujinx");' src/Ryujinx.Graphics.Gpu/Window.cs
    grep -F 'SemuRenderHookProof("ryujinx");' src/Ryujinx.Graphics.OpenGL/Window.cs
    grep -F 'SemuRenderHookProof("ryujinx");' src/Ryujinx.Graphics.GAL/Multithreading/Commands/Window/WindowPresentCommand.cs
    grep -F 'SemuRenderHookComposeFrame(' src/Ryujinx.Graphics.Gpu/Window.cs
    grep -F 'SEMU_RENDER_SHADERS' src/Ryujinx.Graphics.Gpu/Window.cs
    grep -F 'SEMU_RENDER_BEZELS' src/Ryujinx.Graphics.Gpu/Window.cs
    grep -F 'SEMU_RENDER_HOOK_CONFIG' src/Ryujinx.Graphics.Gpu/Window.cs
    grep -F 'SEMU_RENDER_HOOK_PROOF' src/Ryujinx.Graphics.Gpu/Window.cs
    grep -F 'SEMU_RENDER_HOOK_PROOF' src/Ryujinx.Graphics.OpenGL/Window.cs
    grep -F 'SEMU_RENDER_HOOK_PROOF' src/Ryujinx.Graphics.GAL/Multithreading/Commands/Window/WindowPresentCommand.cs
    grep -F 'semu-render-hook:{0}:game_framebuffer:config={1}{2}' src/Ryujinx.Graphics.Gpu/Window.cs
  '';
  sourceHook = {
    status = "source-package-ready";
    requiresSourceBuild = true;
    productionPatchable = true;
    sourcePackagePath = "pkgs.ryubing";
    sourcePlatforms = [ "x86_64-linux" ];
    binaryPackagePaths = [ "../../../build/packaging/nix/ryujinx.nix" ];
    binaryPackageStatus = "legacy-no-source-hook";
    contractProof = {
      marker = "semu-render-hook:ryujinx:game_framebuffer";
      configEnv = "SEMU_RENDER_HOOK_CONFIG";
      proofEnv = "SEMU_RENDER_HOOK_PROOF";
      usesNativeShaderMechanism = false;
      patchFiles = [
        "../../../build/packaging/nix/patches/ryujinx/001-semu-render-hook.patch"
      ];
      patchedFiles = [
        "src/Ryujinx.Graphics.Gpu/Window.cs"
      ];
    };
    remainingGap = null;
  };
  runtimeHookAssertions = ''
    hookDll="$out/lib/ryubing/Ryujinx.Graphics.Gpu.dll"
    if [ ! -f "$hookDll" ]; then
      echo "Semu Ryujinx hook assertion failed: missing $hookDll" >&2
      exit 1
    fi
    if ! grep -aF 'SemuRenderHookComposeFrame' "$hookDll" >/dev/null; then
      echo "Semu Ryujinx hook assertion failed: runtime Ryujinx.Graphics.Gpu.dll lacks SemuRenderHookComposeFrame" >&2
      exit 1
    fi
    openGlHookDll="$out/lib/ryubing/Ryujinx.Graphics.OpenGL.dll"
    if [ ! -f "$openGlHookDll" ]; then
      echo "Semu Ryujinx hook assertion failed: missing $openGlHookDll" >&2
      exit 1
    fi
    if ! grep -aF 'SemuRenderHookProof' "$openGlHookDll" >/dev/null; then
      echo "Semu Ryujinx hook assertion failed: runtime Ryujinx.Graphics.OpenGL.dll lacks SemuRenderHookProof" >&2
      exit 1
    fi
    galHookDll="$out/lib/ryubing/Ryujinx.Graphics.GAL.dll"
    if [ ! -f "$galHookDll" ]; then
      echo "Semu Ryujinx hook assertion failed: missing $galHookDll" >&2
      exit 1
    fi
    if ! grep -aF 'SemuRenderHookProof' "$galHookDll" >/dev/null; then
      echo "Semu Ryujinx hook assertion failed: runtime Ryujinx.Graphics.GAL.dll lacks SemuRenderHookProof" >&2
      exit 1
    fi
  '';
  mergedExtraAttrs = extraAttrs // {
    doCheck = false;
    postInstallAssertions = (extraAttrs.postInstallAssertions or "") + "\n" + runtimeHookAssertions;
  };
in
semuPackage.mkSourceOverride {
  id = "ryujinx";
  base = pkgs.ryubing;
  inherit source includeBasePatches;
  patches = semuPatches ++ patches;
  postPatchAssertions = semuPatchAssertions + "\n" + postPatchAssertions;
  extraAttrs = semuPackage.sourceHookExtraAttrs {
    id = "ryujinx";
    inherit sourceHook;
    extraAttrs = mergedExtraAttrs;
  };
}
