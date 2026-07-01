# RetroArch built from source with the Semu tap patch (STEP 2 reference implementation).
#
# RetroArch reports its real content viewport + active state to the Semu compositor via
# semu_tap.h (see packaging/deck-tap/). No pixel hacks. Cross-built for the Steam Deck
# (x86_64-linux) and run there via nixGL (the flake already uses nixGL for GL on the Deck).
#
# Build:  nix build -f packaging/nix/retroarch-semu.nix
# (cross from aarch64-darwin -> x86_64-linux; no remote builder needed.)
#
# NOTE: the patch (packaging/deck-tap/retroarch-semu-tap.patch) is authored against the
# RetroArch present path (video_driver_frame). If it doesn't apply to the pinned nixpkgs
# RetroArch, pin `src` to the RA commit the patch was written against (master at time of
# writing) via overrideAttrs { src = fetchFromGitHub {...}; }.

{ system ? "x86_64-linux" }:

let
  pkgs  = import <nixpkgs> { config.allowUnsupportedSystem = true; };
  cross = pkgs.pkgsCross.gnu64;            # build on darwin, target x86_64-linux
  tap   = ../deck-tap;
in
cross.retroarch-bare.overrideAttrs (old: {
  pname = "retroarch-semu";

  patches = (old.patches or [ ]) ++ [ "${tap}/retroarch-semu-tap.patch" ];

  # the patch's `#include "semu_tap.h"` (in gfx/video_driver.c) resolves to gfx/semu_tap.h
  postPatch = (old.postPatch or "") + ''
    cp ${tap}/semu_tap.h gfx/semu_tap.h
  '';

  # enable the tap report block
  NIX_CFLAGS_COMPILE = (old.NIX_CFLAGS_COMPILE or "") + " -DHAVE_SEMU_TAP";
})
