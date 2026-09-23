{
  description = "Semu build of libretro core melonds 0-unstable-66b5d2634c from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:libretro/melonds/66b5d2634cd0a79030562811e6e05f5532f800ba"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = import nixpkgs { inherit system; config.allowUnfreePredicate = package: lib.hasPrefix "libretro-" (lib.getName package); }; in
        pkgs.libretro.melonds.overrideAttrs (previous: {
          version = "0-unstable-66b5d2634c";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          includeRetroArch = false;  # Semu wraps RetroArch itself; the nixpkgs launcher would drag in a Darwin-broken RetroArch
          env = (previous.env or { }) // lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux { NIX_LDFLAGS = (previous.env.NIX_LDFLAGS or "") + " -z noexecstack"; };  # an assembly object lacks .note.GNU-stack; glibc refuses to dlopen an executable-stack core
        });
    in {
      semu = {
        id = "melonds";
        version = "0-unstable-66b5d2634c";
        inherit platforms;
        source = { url = "github:libretro/melonds/66b5d2634cd0a79030562811e6e05f5532f800ba"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
