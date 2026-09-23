# The Mega Bezel tree the bezel tools read: the pinned slang shaders beside every upstream pack in
# config/assets/bezels.json, laid out as RetroArch sees them (packs reference ../../shaders_slang).
{ lib, runCommand, fetchFromGitHub, repositoryRoot }:

let
  bezelManifest = lib.importJSON (repositoryRoot + "/config/assets/bezels.json");
  shaderManifest = lib.importJSON (repositoryRoot + "/config/assets/shaders.json");
  fetch = spec: fetchFromGitHub { inherit (spec) owner repo rev; hash = spec.nar_hash; };
  packs = lib.filterAttrs (_: spec: spec.kind == "github" && spec ? pack) bezelManifest.upstreams;
  slang = fetch shaderManifest.upstreams.libretro_slang;
  root = "share/semu/bezel/shaders";
in
runCommand "semu-bezel-tree" { passthru = { inherit slang; packs = lib.mapAttrs (_: fetch) packs; }; } ''
  mkdir -p "$out/${root}/Mega_Bezel_Packs"
  ln -s ${slang} "$out/${root}/shaders_slang"
  ${lib.concatStrings (lib.mapAttrsToList (_: spec: ''
    ln -s ${fetch spec} "$out/${root}/Mega_Bezel_Packs/${spec.pack}"
  '') packs)}
''
