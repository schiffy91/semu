{
  description = "Semu build of Ryujinx (Ryubing) 1.3.3 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "git+https://git.ryujinx.app/projects/Ryubing?ref=refs/tags/1.3.3"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        pkgs.ryubing.overrideAttrs (previous: {
          version = "1.3.3";
          src = source // { tag = "1.3.3"; };  # the recipe reads the release tag from it
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
        });
    in {
      semu = {
        id = "ryujinx";
        version = "1.3.3";
        inherit platforms;
        source = { url = "git+https://git.ryujinx.app/projects/Ryubing?ref=refs/tags/1.3.3"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
