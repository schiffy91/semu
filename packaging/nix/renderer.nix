# The shared renderer every hooked emulator links: shaders through librashader, bezels, the Semu overlay.
{ lib, stdenv, btrcpy, librashader, writeText, repositoryRoot }:

let
  rendererRoot = repositoryRoot + "/src/renderer";
  rendererSource = lib.fileset.toSource {
    root = repositoryRoot;
    fileset = rendererRoot;
  };
  rendererHeader = import (rendererRoot + "/semu_render_header.nix") { inherit writeText; };
in
stdenv.mkDerivation {
  pname = "semu-renderer";
  version = "3";
  src = rendererSource;
  dontConfigure = true;
  strictDeps = true;
  nativeBuildInputs = [ btrcpy ];
  buildInputs = [ librashader ];

  buildPhase = ''
    cd src/renderer
    btrcpy libsemurenderer.btrc -o semu_renderer.c --strict-imports --no-cache --no-stdlib --no-dce
    $CC -c semu_renderer.c -o semu_renderer.o -std=c11 -O2 -fPIC -Wall -Wno-unused-function \
      -DLIBRA_RUNTIME_OPENGL=1 -DSTB_IMAGE_IMPLEMENTATION -DSTBI_ONLY_PNG -I${librashader}/include -I.
    cat > exports.map <<'MAP'
    { global: semu_render_context_invalidate_gl; semu_render_game_gl; semu_render_post_ui_gl; local: *; };
    MAP
    $CC -shared -Wl,-z,defs -Wl,-soname,libsemurenderer.so -Wl,--version-script=exports.map \
      semu_renderer.o -L${librashader}/lib -Wl,-rpath,${librashader}/lib -lrashader -lm -o libsemurenderer.so
  '';

  installPhase = ''
    mkdir -p "$out/include" "$out/lib"
    cp ${rendererHeader} "$out/include/semu_renderer.h"
    cp libsemurenderer.so "$out/lib/libsemurenderer.so"
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    nm -D --defined-only "$out/lib/libsemurenderer.so" | awk '{ print $3 }' | sort > actual
    printf '%s\n' semu_render_context_invalidate_gl semu_render_game_gl semu_render_post_ui_gl | sort > expected
    cmp expected actual
  '';

  passthru = { abi = 3; header = rendererHeader; };
  meta.description = "Semu direct in-process OpenGL renderer";
}
