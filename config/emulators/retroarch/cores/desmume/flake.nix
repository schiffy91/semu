{
  description = "Semu build of libretro core desmume 0-unstable-8f6b32cb9a from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:libretro/desmume/8f6b32cb9a5e310bd38520e7087ce7fa14765f15"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = import nixpkgs { inherit system; config.allowUnfreePredicate = package: lib.hasPrefix "libretro-" (lib.getName package); }; in
        pkgs.libretro.desmume.overrideAttrs (previous: {
          version = "0-unstable-8f6b32cb9a";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          includeRetroArch = false;  # Semu wraps RetroArch itself; the nixpkgs launcher would drag in a Darwin-broken RetroArch
        } // lib.optionalAttrs pkgs.stdenv.hostPlatform.isDarwin {
          env = (previous.env or { }) // { NIX_CFLAGS_COMPILE = ((previous.env or { }).NIX_CFLAGS_COMPILE or "") + " -mmacosx-version-min=11.0"; };  # aligned deallocation needs macOS 10.13 or newer
        });
    in {
      semu = {
        id = "desmume";
        version = "0-unstable-8f6b32cb9a";
        inherit platforms;
        source = { url = "github:libretro/desmume/8f6b32cb9a5e310bd38520e7087ce7fa14765f15"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
