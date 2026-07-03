# dolphin.nix — Dolphin BUILT FROM SOURCE, owned next to its contract
# (emulator.json declares the macos native backend this satisfies; the linux
# contract routes Dolphin through flatpak, never nix). Owner directive:
# source builds so Semu patches — the tap's gameplay-vs-menu active flag
# (rendering/tap/semu_tap.h) — can compile in.
#
# The source is Semu's own pin of the current stable tag; the build wiring
# (dependency set, cmake flags, the darwin Dolphin.app install) is inherited
# from nixpkgs dolphin-emu (a proven darwin source recipe), overridden onto
# this pin.
{ lib, stdenv, fetchFromGitHub, dolphin-emu }:

let
  version = "2603a";

  # ONE pinned source: tag 2603a (annotated tag peeled to its commit),
  # resolved 2026-07-02 via git ls-remote — pin the commit, not the movable
  # tag. Submodules carry the Externals/ tree upstream builds against, and
  # the postFetch COMMIT file feeds the recipe's preConfigure version stamp
  # (same trick as nixpkgs; the hash matches its tag fetch).
  source = fetchFromGitHub {
    owner = "dolphin-emu";
    repo = "dolphin";
    rev = "5e7cc91d8c9a43ca189b288937f65c9763af9c22";
    fetchSubmodules = true;
    leaveDotGit = true;
    postFetch = ''
      pushd $out
      git rev-parse HEAD 2>/dev/null >$out/COMMIT
      find $out -name .git -print0 | xargs -0 rm -rf
      popd
    '';
    hash = "sha256-+3/JtjKFsTEkKQa0LjycqNmDz0M8o2FndWQtw5R5/jQ=";
  };

  # Semu patches hook in here.
  patches = [ ];

  sourceBuild = dolphin-emu.overrideAttrs (previous: {
    inherit version;
    src = source;
    patches = (previous.patches or [ ]) ++ patches;
  });
in
if stdenv.hostPlatform.isDarwin then sourceBuild
else throw "dolphin.nix: unsupported platform ${stdenv.hostPlatform.system} (the linux contract routes Dolphin through flatpak, never nix)"
