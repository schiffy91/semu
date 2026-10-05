{
  description = "Semu build of Azahar 2126.0 from its pinned upstream source";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "git+https://github.com/azahar-emu/azahar?ref=refs/tags/2126.0&submodules=1"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = true; windows = "planned"; };  # windows: nothing built yet
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        pkgs.azahar.overrideAttrs (previous: {
          version = "2126.0";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          patches = (previous.patches or [ ]) ++ [
            ./semu-touch.patch  # touch on the composed picture lands where the bottom screen is shown
            ./semu-joystick-lock-order.patch  # no deadlock between the joystick map and SDL at start
          ];
          postUnpack = (previous.postUnpack or "") + ''
            echo "2126.0" > "$sourceRoot/GIT-TAG"
            echo "${source.rev}" > "$sourceRoot/GIT-COMMIT"
          '';  # the nixpkgs fetch wrote these; the flake input carries no .git
        } // lib.optionalAttrs (pkgs.stdenv.hostPlatform.isLinux && pkgs.stdenv.hostPlatform.isx86_64) {
          # the Deck's CPU level, which upstream's ENABLE_NATIVE_OPTIMIZATION (march native, CMakeLists.txt:158, 252-266) gives on
          # the Deck itself, held at x86-64-v3 so FRACTAL-NORTH runs the same build; Release and LTO are upstream's defaults already
          env = (previous.env or { }) // { NIX_CFLAGS_COMPILE = ((previous.env or { }).NIX_CFLAGS_COMPILE or "") + " -march=x86-64-v3 -mtune=znver2"; };
          preConfigure = (previous.preConfigure or "") + ''
            awk '/__AVX2__/ { avx = 1 } /__tune_znver2__/ { tune = 1 } END { print "compiler CPU level: AVX2 " avx ", tuned for znver2 " tune; exit !(avx && tune) }' < <($CXX -dM -E -x c++ /dev/null)
          '';  # every compile carries the CPU level (printed in the build log)
        } // lib.optionalAttrs pkgs.stdenv.hostPlatform.isDarwin {
          buildInputs = (previous.buildInputs or [ ]) ++ [ pkgs.moltenvk ];  # upstream downloads MoltenVK at configure time; the sandbox has no network
          cmakeFlags = (previous.cmakeFlags or [ ]) ++ [ "-DUSE_SYSTEM_MOLTENVK=ON" "-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0" ];  # the Nix libraries target macOS 14
          env = (previous.env or { }) // { NIX_LDFLAGS = ((previous.env or { }).NIX_LDFLAGS or "") + " -framework QuartzCore"; };  # CAMetalLayer for the Vulkan surface
          postPatch = (previous.postPatch or "") + ''
            substituteInPlace src/citra_qt/citra_qt.cpp --replace-fail 'if (AppleUtils::IsRunningFromTerminal()) {' 'if (false) {'
          '';  # Semu starts azahar as its child: without this, every launch stops at a "run it through open" dialog
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
