{
  description = "Semu build of Cemu 2.6 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "github:cemu-project/Cemu/v2.6"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = false; windows = "planned"; };  # windows: nothing built yet, declared so the matrix is explicit
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        pkgs.cemu.overrideAttrs (previous: {
          version = "2.6";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          patches = (previous.patches or [ ]) ++ [
            ./semu-fullscreen-at-boot.patch  # a -g -f launch is full screen and black from the start, never the menu-bar window
            ./semu-no-shader-progress.patch  # the shader cache's loading screen shows the game's boot image only (M16 OSD review)
            ./semu-h264-stream-recovery.patch  # retain decoder state when video playback falls behind
          ];
          # upstream's release is CMake's own Release flags, -O3 -DNDEBUG, with LTO (CMakeLists.txt:73-75, build.yml:69 at v2.6);
          # the nixpkgs recipe sets the Release flags to -DNDEBUG alone, which leaves the hardening wrapper's -O2
          cmakeFlags = lib.filter (flag: !(lib.hasInfix "_FLAGS_RELEASE" flag)) (previous.cmakeFlags or [ ]);
          postConfigure = (previous.postConfigure or "") + ''
            compileFlags="$(grep -h '^  FLAGS = ' build.ninja)"
            awk '/-O3 -DNDEBUG/ && /-flto/ { print; found = 1; exit } END { exit !found }' <<<"$compileFlags"  # the release flags reached the compiler (printed in the build log)
          '';
        });
    in {
      semu = {
        id = "cemu";
        version = "2.6";
        inherit platforms;
        source = { url = "github:cemu-project/Cemu/v2.6"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
