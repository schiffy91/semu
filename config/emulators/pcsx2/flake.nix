{
  description = "Semu build of PCSX2 2.6.3 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:PCSX2/pcsx2/v2.6.3"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = false; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        pkgs.pcsx2.overrideAttrs (previous: {
          version = "2.6.3";
          src = source // { tag = "v2.6.3"; };  # the recipe stamps PCSX2_GIT_TAG from it
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
        });
    in {
      semu = {
        id = "pcsx2";
        version = "2.6.3";
        inherit platforms;
        source = { url = "github:PCSX2/pcsx2/v2.6.3"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
