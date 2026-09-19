# Dolphin: nixpkgs lends the build wiring; the source is Semu's own pin and it is always built here.
# The recipe stamps DOLPHIN_WC_REVISION from a COMMIT file the fetch leaves behind.
{ lib, fetchFromGitHub, dolphin-emu }:
let contract = lib.importJSON ./package.json; source = contract.source; in
dolphin-emu.overrideAttrs (previous: {
  version = contract.version;
  src = fetchFromGitHub {
    inherit (source) owner repo tag;
    hash = source.sha256;
    fetchSubmodules = true;
    leaveDotGit = true;
    postFetch = ''
      pushd $out
      git rev-parse HEAD 2>/dev/null >$out/COMMIT
      find $out -name .git -print0 | xargs -0 rm -rf
      popd
    '';
  };
  allowSubstitutes = false;  # never a cache binary
})
