{
  description = "Semu build of libretro core snes9x 0-unstable-a0420aef6a from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:snes9xgit/snes9x/a0420aef6a5df627c1ddd00495f3eb32b6ef8aa3"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = import nixpkgs { inherit system; config.allowUnfreePredicate = package: lib.hasPrefix "libretro-" (lib.getName package); }; in
        pkgs.libretro.snes9x.overrideAttrs (previous: {
          version = "0-unstable-a0420aef6a";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          includeRetroArch = false;  # Semu wraps RetroArch itself; the nixpkgs launcher would drag in a Darwin-broken RetroArch
        });
    in {
      semu = {
        id = "snes9x";
        version = "0-unstable-a0420aef6a";
        inherit platforms;
        source = { url = "github:snes9xgit/snes9x/a0420aef6a5df627c1ddd00495f3eb32b6ef8aa3"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
