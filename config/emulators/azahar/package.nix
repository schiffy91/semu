# Azahar: nixpkgs lends the build wiring; the source is Semu's own pin and it is always built here.
# The fetch initialises the submodule subset the build needs and leaves GIT-TAG and GIT-COMMIT for the version stamp.
{ lib, fetchFromGitHub, azahar }:
let contract = lib.importJSON ./package.json; source = contract.source; in
azahar.overrideAttrs (previous: {
  version = contract.version;
  src = fetchFromGitHub {
    inherit (source) owner repo tag;
    hash = source.sha256;
    postCheckout = ''
      git -C "$out/externals" submodule update --init \
        teakra zstd discord-rpc spirv-headers spirv-tools sirit xxHash \
        faad2/faad2 lodepng/lodepng dds-ktx nihstro "$out/dist/compatibility_list"
      echo "${source.tag}" > "$out/GIT-TAG"
      git -C "$out" rev-parse HEAD > "$out/GIT-COMMIT"
    '';
  };
  allowSubstitutes = false;  # never a cache binary
})
