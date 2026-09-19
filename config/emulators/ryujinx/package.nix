# ryujinx: nixpkgs lends the build wiring; the source is Semu's own pin in package.json and it is always built here.
{ lib, fetchFromGitHub, fetchFromForgejo, ryubing }:
let
  contract = lib.importJSON ./package.json;
  pinned = import ../../../packaging/nix/pinned_source.nix { inherit lib fetchFromGitHub fetchFromForgejo; };
in
ryubing.overrideAttrs (previous: {
  version = contract.version;
  src = pinned contract.source;
  allowSubstitutes = false;  # never a cache binary
})
