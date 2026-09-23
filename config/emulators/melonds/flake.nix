{
  description = "Semu build of melonDS 1.1-unstable-2026-08-24 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:melonDS-emu/melonDS/906e9ebb27da8c6a715cd7abab4abfe8a8d29427"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = false; windows = "planned"; };  # macos: no emulator.json slice launches it; windows: nothing built yet
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        pkgs.melonds.overrideAttrs (previous: {
          version = "1.1-unstable-2026-08-24";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
        });
    in {
      semu = {
        id = "melonds";
        version = "1.1-unstable-2026-08-24";
        inherit platforms;
        source = { url = "github:melonDS-emu/melonDS/906e9ebb27da8c6a715cd7abab4abfe8a8d29427"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
