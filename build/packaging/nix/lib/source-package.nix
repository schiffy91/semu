{ lib, fetchFromGitHub, applyPatches }:

let
  sourceFrom = source:
    if source == null then null
    else if lib.isAttrs source && (source.type or null) == "github" then fetchFromGitHub {
      owner = source.owner;
      repo = source.repo;
      rev = source.rev;
      hash = source.hash;
    }
    else if lib.isAttrs source && !(lib.isDerivation source) then
      throw "Unsupported Semu source type: ${source.type or "<missing>"}"
    else source;

  joinNonEmpty = strings:
    lib.concatStringsSep "\n" (lib.filter (s: s != "") strings);

  semuPassthru = { id, sourceHook, passthru ? {} }:
    passthru // {
      semu = (passthru.semu or {}) // {
        inherit id sourceHook;
      };
    };
in
{
  sourceHookExtraAttrs = { id, sourceHook, extraAttrs ? {} }:
    extraAttrs // {
      passthru = semuPassthru {
        inherit id sourceHook;
        passthru = extraAttrs.passthru or {};
      };
    };

  withSourceHookMetadata = { id, package, sourceHook }:
    package.overrideAttrs (old: {
      passthru = semuPassthru {
        inherit id sourceHook;
        passthru = old.passthru or {};
      };
    });

  mkSourceOverride = {
    id,
    base,
    source ? null,
    patches ? [],
    includeBasePatches ? source == null,
    postPatchAssertions ? "",
    extraAttrs ? {},
  }:
    if source == null && patches == [] && postPatchAssertions == "" && extraAttrs == {} then base
    else base.overrideAttrs (old:
      let
        oldPassthru = old.passthru or {};
        oldSemu = oldPassthru.semu or {};
        extraPassthru = extraAttrs.passthru or {};
        extraSemu = extraPassthru.semu or {};
        extraPostPatch = extraAttrs.postPatch or "";
        extraPostInstall = extraAttrs.postInstall or "";
        postInstallAssertions = extraAttrs.postInstallAssertions or "";
        basePatches = lib.optionals includeBasePatches (old.patches or []);
        allPatches = basePatches ++ patches;
        upstreamSource = if source == null then old.src else sourceFrom source;
        patchedSource = if allPatches == [] then upstreamSource else applyPatches {
          name = "${id}-semu-patched-source";
          src = upstreamSource;
          patches = allPatches;
        };
        patchCheck = applyPatches {
          name = "${id}-semu-patch-check";
          src = upstreamSource;
          patches = allPatches;
          postPatch = postPatchAssertions;
        };
        passthruTests = (oldPassthru.tests or {}) // (extraPassthru.tests or {})
          // lib.optionalAttrs (allPatches != [] || postPatchAssertions != "") {
            semuPatchesApply = patchCheck;
          };
      in
        builtins.removeAttrs extraAttrs [ "passthru" "patches" "postPatch" "postInstall" "postInstallAssertions" "src" ] // {
          src = patchedSource;
          patches = [];
          postPatch = joinNonEmpty [
            (old.postPatch or "")
            extraPostPatch
            postPatchAssertions
          ];
          postInstall = joinNonEmpty [
            (old.postInstall or "")
            extraPostInstall
            postInstallAssertions
          ];
          passthru = oldPassthru // extraPassthru // {
            semu = oldSemu // extraSemu // {
              inherit id source patches includeBasePatches;
              sourceOverride = source;
              basePatchCount = builtins.length basePatches;
            };
            tests = passthruTests;
          };
        });
}
