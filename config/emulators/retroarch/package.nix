# RetroArch built from Semu's pinned source with the render hook: gl3 calls the
# shared renderer after the game draw and before present. `withCores` wraps it
# with the core packages the system bindings select.
{ lib, pkgs, retroarch-bare, makeBinaryWrapper, writeText, symlinkJoin, btrcpy, semuRendererLoader, bridgeSource, source, version, retroarch-assets, libretro-core-info }:

let
  hooked = retroarch-bare.overrideAttrs (previous: {
    pname = "retroarch-semu";
    inherit version;
    src = source;
    allowSubstitutes = false;  # compiled by Semu, never a cache binary
    patches = (previous.patches or [ ]) ++ [ ./retroarch.patch ./retroarch_commands.patch ./retroarch_get_status_null_safety.patch ];
    patchFlags = [ "-p1" "--fuzz=0" ];  # a patch that drifted from the pinned source fails instead of landing fuzzily
    nativeBuildInputs = (previous.nativeBuildInputs or [ ]) ++ [ btrcpy ];
    buildInputs = (previous.buildInputs or [ ]) ++ [ semuRendererLoader ];  # the loader: renderer changes never rebuild RetroArch
    postPatch = (previous.postPatch or "") + ''
      btrcpy ${bridgeSource}/runtime_bridge.btrc -o gfx/semu_retroarch.c \
        --strict-imports --no-cache --no-stdlib --no-dce
      test -s gfx/semu_retroarch.c
      # Software cores get a 3.2 core context by default; the renderer and librashader need 3.3 / GLSL 330.
      sed -i '/major = 3;/{n;s/minor = 2;/minor = 3;/}' gfx/drivers/gl3.c
      grep -Fq 'minor = 3;' gfx/drivers/gl3.c
      # Cores with version-gated loaders (Citra's glad) see only 3.3 entry points in a 3.3 context; give core-profile requests 4.6.
      sed -i '/gl_query_core_context_set(hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE);/a\      if (hwr->context_type == RETRO_HW_CONTEXT_OPENGL_CORE \&\& major == 3) { major = 4; minor = 6; }' gfx/drivers/gl3.c
      grep -Fq 'major = 4; minor = 6;' gfx/drivers/gl3.c
    '';
    env = (previous.env or { }) // {
      NIX_CFLAGS_COMPILE = (previous.env.NIX_CFLAGS_COMPILE or "") + " -DHAVE_SEMU_RENDERER -I${semuRendererLoader}/include";
      NIX_LDFLAGS = (previous.env.NIX_LDFLAGS or "") + " -L${semuRendererLoader}/lib --whole-archive -lsemurendererloader --no-whole-archive -ldl";
    };
    postInstall = (previous.postInstall or "") + ''
      ! grep -Fq 'libsemurenderer.so' <<<"$(readelf -d "$out/bin/retroarch")"  # loaded at run time, never linked
      grep -Fq semu-renderer-loader "$out/bin/retroarch"  # the loader is linked in
    '';
    passthru = (previous.passthru or { }) // {
      withCores = cores:  # nixpkgs' wrapper with exactly these core packages
        (import (pkgs.path + "/pkgs/by-name/re/retroarch-bare/wrapper.nix") {
          inherit lib makeBinaryWrapper writeText symlinkJoin;
          libretro = { };
          retroarch-bare = hooked;
          inherit cores;
          settings = {  # no joypad_autoconfig_dir: this appendconfig would override the profile's bundle dir with the Deck profiles
            assets_directory = "${retroarch-assets}/share/retroarch/assets";
            libretro_info_path = "${libretro-core-info}/share/retroarch/cores";
          };
        }).overrideAttrs (wrapped: { passthru = (wrapped.passthru or { }) // { unwrapped = hooked; }; });
    };
  });
in
hooked
