{
  description = "Semu build of Dolphin 2606a from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "git+https://github.com/dolphin-emu/dolphin?ref=refs/tags/2606a&submodules=1"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        pkgs.dolphin-emu.overrideAttrs (previous: {
          version = "2606a";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          postUnpack = (previous.postUnpack or "") + ''
            echo "${source.rev}" > "$sourceRoot/COMMIT"
          '';  # preConfigure stamps DOLPHIN_WC_REVISION from it
        });
    in {
      semu = {
        id = "dolphin";
        version = "2606a";
        inherit platforms;
        source = { url = "git+https://github.com/dolphin-emu/dolphin?ref=refs/tags/2606a&submodules=1"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
