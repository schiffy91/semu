{ lib }:

{
  emulatorDir,
}:

let
  emulatorId = builtins.baseNameOf (toString emulatorDir);
  packageFile = emulatorDir + "/package.json";
  renderingFile = emulatorDir + "/rendering.json";
  packageContract = builtins.fromJSON (builtins.readFile packageFile);
  packageFallbacks = packageContract.fallbacks or { };
  enabledFallbacks = lib.filter
    (name: packageFallbacks.${name} != false)
    (builtins.attrNames packageFallbacks);
  contract = builtins.fromJSON (builtins.readFile renderingFile);
  integration = contract.integration;
  composition = contract.composition or { };
  phases = contract.phases or [ ];
  hook = integration // { inherit phases; };
  gamePhase = lib.findFirst (phase: phase.id == "game") null phases;
  postUiPhase = lib.findFirst (phase: phase.id == "post_ui") null phases;
  firstPhase = if phases == [ ] then null else builtins.elemAt phases 0;
  secondPhase = if builtins.length phases < 2 then null else builtins.elemAt phases 1;
  patchPath = emulatorDir + "/${integration.patch}";
  # Keep the patch as a flake-source path. Re-importing it with builtins.path
  # creates an unreferenced store path that can disappear during parallel
  # all-system evaluation before overrideAttrs consumes it.
  patch = patchPath;
  patchText = builtins.readFile patch;
  addedPatchText = lib.concatStringsSep "\n" (lib.filter
    (line: lib.hasPrefix "+" line && !lib.hasPrefix "+++" line)
    (lib.splitString "\n" patchText));

  obsoleteCapabilities = lib.filter (name: builtins.hasAttr name integration) [
    "interposition"
    "gamescope"
    "x11"
    "wayland"
    "preload"
    "capture_window"
    "runtime_renderer_symbol_discovery"
  ];
  forbiddenPatchTerms = lib.filter (term: lib.hasInfix term addedPatchText) [
    "HAVE_SEMU_TAP"
    "LD_PRELOAD"
    "NativeLibrary.GetExport"
    "PFNGLXGETPROCADDRESSPROC"
    "SEMU_TAP"
    "XGetImage"
    "_glXGetProcAddress"
    "capture_window"
    "dlopen"
    "dlsym"
    "eglSwapBuffers"
    "gamescope"
    "glXSwapBuffers"
    "semu_tap"
    "wl_shm"
    "GetProcAddress(\"semu_"
  ];
in
assert lib.assertMsg (contract.schema_version == 1)
  "${emulatorId}: unsupported rendering contract schema";
assert lib.assertMsg
  (builtins.attrNames packageFallbacks != [ ] && enabledFallbacks == [ ])
  "${emulatorId}: every package fallback must be declared and disabled; enabled: ${toString enabledFallbacks}";
assert lib.assertMsg
  (contract.activation.mode == "source_patched_direct_link"
    && contract.activation.package_ready)
  "${emulatorId}: rendering must be an enabled source-patched direct integration";
assert lib.assertMsg (obsoleteCapabilities == [ ])
  "${emulatorId}: obsolete rendering capabilities: ${toString obsoleteCapabilities}";
assert lib.assertMsg
  (builtins.isInt integration.abi
    && integration.abi > 0
    && integration.api == "semu-renderer"
    && lib.elem integration.linkage [ "direct" "direct_pinvoke" ]
    && integration.header == "semu_renderer.h"
    && integration.library == "semurenderer"
    && integration.backend == "opengl"
    && integration.inactive_reports == false)
  "${emulatorId}: rendering integration does not declare a valid Semu ABI contract";
assert lib.assertMsg
  (builtins.length phases == 2
    && firstPhase != null
    && firstPhase.id == "game"
    && firstPhase.symbol == "semu_render_game_gl"
    && secondPhase != null
    && secondPhase.id == "post_ui"
    && secondPhase.symbol == "semu_render_post_ui_gl"
    && gamePhase == firstPhase
    && postUiPhase == secondPhase)
  "${emulatorId}: rendering must declare exact game and post-UI ABI phases";
assert lib.assertMsg
  ((composition.owner or null) == "semurenderer"
    && (composition.shader or null) == "renderer_owned"
    && (composition.bezel or null) == "renderer_owned"
    && (composition.semu_menu or null) == "renderer_owned")
  "${emulatorId}: shader, bezel, and Semu menu composition must remain renderer-owned";
assert lib.assertMsg (builtins.pathExists patchPath)
  "${emulatorId}: declared rendering patch is missing";
assert lib.assertMsg (forbiddenPatchTerms == [ ])
  "${emulatorId}: Semu patch contains obsolete interception logic: ${toString forbiddenPatchTerms}";
{
  inherit
    addedPatchText
    contract
    gamePhase
    hook
    patch
    patchPath
    patchText
    postUiPhase
    ;
  patchHash = builtins.hashFile "sha256" patch;
}
