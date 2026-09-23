# The shared renderer every hooked emulator links: shaders through librashader, bezels, the Semu overlay.
{ lib, stdenv, btrcpy, librashader, writeText, rendererRoot }:

let
  rendererSource = lib.fileset.toSource {
    root = rendererRoot;
    fileset = rendererRoot;
  };
  rendererHeader = import (rendererRoot + "/semu_render_header.nix") { inherit writeText; };
in
stdenv.mkDerivation {
  pname = "semu-renderer";
  version = "3";
  src = rendererSource;
  dontConfigure = true;
  allowSubstitutes = false;  # compiled by Semu, never a cache binary
  strictDeps = true;
  nativeBuildInputs = [ btrcpy ];
  buildInputs = [ librashader ];

  buildPhase = ''
    btrcpy libsemurenderer.btrc -o semu_renderer.c --strict-imports --no-cache --no-stdlib --no-dce
    $CC -c semu_renderer.c -o semu_renderer.o -std=c11 -O2 -fPIC -Wall -Wno-unused-function \
      -DLIBRA_RUNTIME_OPENGL=1 -DSTB_IMAGE_IMPLEMENTATION -DSTBI_ONLY_PNG -DSTBI_ONLY_JPEG -I${librashader}/include -I.
  '' + (if stdenv.hostPlatform.isDarwin then ''
    printf '%s\n' _semu_render_context_invalidate_gl _semu_render_game_gl _semu_render_post_ui_gl > exports.txt
    $CC -dynamiclib -Wl,-exported_symbols_list,exports.txt -install_name "$out/lib/libsemurenderer.dylib" \
      semu_renderer.o -L${librashader}/lib -Wl,-rpath,${librashader}/lib -lrashader -lm -o libsemurenderer.dylib
  '' else ''
    cat > exports.map <<'MAP'
    { global: semu_render_context_invalidate_gl; semu_render_game_gl; semu_render_post_ui_gl; local: *; };
    MAP
    $CC -shared -Wl,-z,defs -Wl,-soname,libsemurenderer.so -Wl,--version-script=exports.map \
      semu_renderer.o -L${librashader}/lib -Wl,-rpath,${librashader}/lib -lrashader -lm -o libsemurenderer.so
    # LD_PRELOAD shim for SDL2 + OpenGL emulators: composes at SDL_GL_SwapWindow.
    cp ${rendererHeader} semu_renderer.h
    btrcpy preload/semu_preload.btrc -o semu_preload.c --strict-imports --no-cache --no-stdlib --no-dce
    $CC -c semu_preload.c -o semu_preload.o -std=c11 -O2 -fPIC -Wall -Wno-unused-function -D_GNU_SOURCE -I.
    cat > preload.map <<'MAP'
    { global: SDL_GL_SwapWindow; eglSwapBuffers; glXSwapBuffers; local: *; };
    MAP
    $CC -shared -Wl,-soname,libsemupreload.so -Wl,--version-script=preload.map \
      semu_preload.o -L. -Wl,-rpath,$out/lib -lsemurenderer -ldl -o libsemupreload.so
  '');

  installPhase = ''
    mkdir -p "$out/include" "$out/lib"
    cp ${rendererHeader} "$out/include/semu_renderer.h"
  '' + (if stdenv.hostPlatform.isDarwin then ''
    cp libsemurenderer.dylib "$out/lib/libsemurenderer.dylib"
    ln -s libsemurenderer.dylib "$out/lib/libsemurenderer.so"  # one SEMU_RENDERER_LIBRARY path on every platform
  '' else ''
    cp libsemurenderer.so "$out/lib/libsemurenderer.so"
    cp libsemupreload.so "$out/lib/libsemupreload.so"
  '');

  doInstallCheck = true;
  installCheckPhase = ''
  '' + lib.optionalString (!stdenv.hostPlatform.isDarwin) ''
    nm -D --defined-only "$out/lib/libsemurenderer.so" | awk '{ print $3 }' | sort > actual
    printf '%s\n' semu_render_context_invalidate_gl semu_render_game_gl semu_render_post_ui_gl | sort > expected
    cmp expected actual
  '' + ''
    btrcpy loader/loader_probe.btrc -o loader_probe.c --strict-imports --no-cache --no-stdlib --no-dce  # emulators link the loader: prove it forwards
    $CC loader_probe.c -std=c11 -O1 -o loader_probe -ldl
    test "$(SEMU_RENDERER_LIBRARY="$out/lib/libsemurenderer.so" ./loader_probe)" = "-1"
    test "$(SEMU_RENDERER_LIBRARY=/nonexistent/libsemurenderer.so ./loader_probe 2>/dev/null)" = "0"
  '';

  passthru = { abi = 3; header = rendererHeader; };
  meta.description = "Semu direct in-process OpenGL renderer";
}
