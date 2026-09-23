{
  description = "Semu build of the Azahar libretro core 2126.0 from the pinned Azahar source (ENABLE_LIBRETRO)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/e554fab72f81915600f3f449b786fd9af40439a5";
    source = { url = "git+https://github.com/azahar-emu/azahar?ref=refs/tags/2126.0&submodules=1"; flake = false; };
  };

  outputs = { self, nixpkgs, source }:
    let
      lib = nixpkgs.lib;
      platforms = { linux = true; macos = false; windows = "planned"; };  # macos: the core compiles its OpenGL renderer out on Apple, and macOS RetroArch presents through glcore for the Semu renderer; windows: nothing built yet
      systems = [ "x86_64-linux" ] ++ lib.optional platforms.macos "aarch64-darwin";
      build = system:
        let pkgs = nixpkgs.legacyPackages.${system}; in
        (pkgs.azahar.override { useDiscordRichPresence = false; }).overrideAttrs (previous: {
          pname = "azahar-libretro";
          version = "2126.0";
          src = source;
          allowSubstitutes = false;  # compiled by Semu, never a cache binary
          dontWrapQtApps = true;
          postUnpack = (previous.postUnpack or "") + ''
            echo "2126.0" > "$sourceRoot/GIT-TAG"
            echo "${source.rev}" > "$sourceRoot/GIT-COMMIT"
          '';  # the nixpkgs fetch wrote these; the flake input carries no .git
          cmakeFlags = [  # the libretro build forces the frontend, audio and network options off; naming them ON is a configure error
            "-DUSE_SYSTEM_LIBS=ON" "-DDISABLE_SYSTEM_LODEPNG=ON" "-DDISABLE_SYSTEM_VMA=ON" "-DDISABLE_SYSTEM_ZSTD=ON" "-DDISABLE_SYSTEM_SPIRV_HEADERS=ON"
            "-DENABLE_LIBRETRO=ON" "-DENABLE_SSE42=ON" "-DENABLE_TESTS=OFF" "-DUSE_DISCORD_PRESENCE=OFF" "-DCITRA_WARNINGS_AS_ERRORS=OFF"
          ];
          installPhase = ''
            runHook preInstall
            core="$(find . -name azahar_libretro.so -o -name azahar_libretro.dylib | head -n 1)"
            test -n "$core"
            mkdir -p "$out/lib/retroarch/cores"
            cp "$core" "$out/lib/retroarch/cores/"
            runHook postInstall
          '';
          passthru = (previous.passthru or { }) // { libretroCore = "/lib/retroarch/cores"; };  # the nixpkgs RetroArch wrapper adds -L for this path
          meta = (removeAttrs (previous.meta or { }) [ "mainProgram" ]) // { description = "Azahar as a libretro core (the citra_libretro target of the Azahar tree)"; };
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
