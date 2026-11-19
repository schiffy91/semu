{
  description = "libsemurenderer and libsemupreload: the one shader and bezel renderer every Semu emulator draws through";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    btrc = {
      url = "github:schiffy91/btrc";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, btrc }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # OpenGL: EGL/GLX on Linux, CGL on macOS; D3D is planned
      systems = [ "x86_64-linux" "aarch64-darwin" ];
    in {
      semu = { id = "renderer"; inherit platforms; };
      # Compiled into RetroArch's gl3 driver by the retroarch flake: the bridge's committed C and the header it names.
      retroarchBridge = lib.fileset.toSource { root = ./retroarch; fileset = lib.fileset.unions [ ./retroarch/generated ./retroarch/semu_render_abi.h ]; };
      packages = lib.genAttrs systems (system:
        let pkgs = nixpkgs.legacyPackages.${system}; in {
          default = pkgs.callPackage ./package.nix { btrcpy = btrc.packages.${system}.btrcpy; rendererRoot = ./.; };
          loader = pkgs.callPackage ./loader.nix { rendererRoot = ./.; };  # what emulators link
          generated = pkgs.callPackage ./generated.nix { btrcpy = btrc.packages.${system}.btrcpy; rendererRoot = ./.; };  # what the committed C must equal
        });
    };
}
