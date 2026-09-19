{
  description = "Semu build of Flycast 2.7 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "git+https://github.com/flyinghead/flycast?ref=refs/tags/v2.7&submodules=1"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        pkgs.flycast.overrideAttrs (previous: {
          version = "2.7";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
        });
    in {
      semu = {
        id = "flycast";
        version = "2.7";
        inherit platforms;
        source = { url = "git+https://github.com/flyinghead/flycast?ref=refs/tags/v2.7&submodules=1"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
