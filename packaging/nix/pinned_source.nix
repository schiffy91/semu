# One pinned upstream source per emulator and core, declared in Semu's JSON contracts.
# The content hash makes the pin immutable; Semu builds from it and never substitutes.
{ lib, fetchFromGitHub, fetchFromForgejo }:
source:
let
  ref = if source ? tag then { tag = source.tag; } else { rev = source.revision; };
  common = ref // {
    inherit (source) owner repo;
    hash = source.sha256;
    fetchSubmodules = source.fetch_submodules or false;
  };
in
assert lib.assertMsg (lib.hasPrefix "sha256-" (source.sha256 or "")) "pinned source needs a sha256";
if (source.kind or "github") == "forgejo" then fetchFromForgejo (common // { inherit (source) domain; })
else fetchFromGitHub (common // lib.optionalAttrs (source ? leave_dot_git) { leaveDotGit = source.leave_dot_git; })
