{ pkgs
, semuPackage ? null
, source ? null
, patches ? []
, includeBasePatches ? source == null
, postPatchAssertions ? ""
, extraAttrs ? {}
}:

let
  base = pkgs.gamescope;
  hasSourceOverride =
    source != null || patches != [] || postPatchAssertions != "" || extraAttrs != {};
in
if semuPackage == null then
  if hasSourceOverride then
    throw "gamescope source overrides require semuPackage"
  else
    base
else
  semuPackage.mkSourceOverride {
    id = "gamescope";
    inherit base source patches includeBasePatches postPatchAssertions extraAttrs;
  }
