{
  description = "Semu build of Azahar 2126.0 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "git+https://github.com/azahar-emu/azahar?ref=refs/tags/2126.0&submodules=1"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = false; windows = "planned"; };  # macos: no emulator.json slice launches it; windows: nothing built yet
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        pkgs.azahar.overrideAttrs (previous: {
          version = "2126.0";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          postUnpack = (previous.postUnpack or "") + ''
            echo "2126.0" > "$sourceRoot/GIT-TAG"
            echo "${source.rev}" > "$sourceRoot/GIT-COMMIT"
          '';  # the nixpkgs fetch wrote these; the flake input carries no .git
        });
    in {
      semu = {
        id = "azahar";
        version = "2126.0";
        inherit platforms;
        source = { url = "git+https://github.com/azahar-emu/azahar?ref=refs/tags/2126.0&submodules=1"; rev = source.rev; narHash = source.narHash; };
      };
      packages = lib.genAttrs systems (system: { default = build system; });
    };
}
