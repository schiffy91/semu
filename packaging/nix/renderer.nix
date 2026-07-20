# Shared renderer package linked directly by source-built emulators.
{
  lib,
  stdenv,
  btrcpy,
  librashader,
  writeText,
  repositoryRoot,
}:

let
  rendererRoot = repositoryRoot + "/src/generators/rendering/native";
  rendererFiles = [
    (rendererRoot + "/libsemurenderer.btrc")
    (rendererRoot + "/librashader_gl.btrc")
    (rendererRoot + "/renderer_abi.btrc")
    (rendererRoot + "/renderer_compositor.btrc")
    (rendererRoot + "/renderer_file_hash.btrc")
    (rendererRoot + "/renderer_gl_api.btrc")
    (rendererRoot + "/renderer_post_ui.btrc")
    (rendererRoot + "/stb_image.h")
    (repositoryRoot + "/src/generators/input/action_abi.btrc")
  ] ++ lib.optionals stdenv.hostPlatform.isLinux [
    (repositoryRoot + "/src/generators/input/linux_supervisor.btrc")
  ];
  rendererSource = lib.fileset.toSource {
    root = repositoryRoot;
    fileset = lib.fileset.unions rendererFiles;
  };
  rendererHeader = import (rendererRoot + "/semu_render_header.nix") {
    inherit writeText;
  };
  abiConformanceSource = writeText "semu-renderer-abi-conformance.c" ''
    #include <stddef.h>
    #include <stdio.h>
    #include <string.h>
    #include <semu_renderer.h>

    _Static_assert(sizeof(SemuRenderSurfaceGl) == 32, "surface size");
    _Static_assert(offsetof(SemuRenderSurfaceGl, origin) == 28, "surface tail");
    _Static_assert(sizeof(SemuRenderPointerSurface) == 32, "pointer surface size");
    _Static_assert(offsetof(SemuRenderPointerSurface, origin) == 28,
      "pointer surface tail");

    _Static_assert(sizeof(SemuRenderPointerMap) == 96, "pointer map size");
    _Static_assert(offsetof(SemuRenderPointerMap, abi) == 0, "map abi");
    _Static_assert(offsetof(SemuRenderPointerMap, struct_size) == 4, "map size field");
    _Static_assert(offsetof(SemuRenderPointerMap, frame_id) == 8, "map frame");
    _Static_assert(offsetof(SemuRenderPointerMap, framebuffer_width) == 16,
      "map width");
    _Static_assert(offsetof(SemuRenderPointerMap, framebuffer_height) == 20,
      "map height");
    _Static_assert(offsetof(SemuRenderPointerMap, surface_count) == 24,
      "map count");
    _Static_assert(offsetof(SemuRenderPointerMap, surfaces) == 28, "map surfaces");

    _Static_assert(sizeof(SemuRenderPointerQuery) == 48, "query size");
    _Static_assert(offsetof(SemuRenderPointerQuery, frame_id) == 8, "query frame");
    _Static_assert(offsetof(SemuRenderPointerQuery, x) == 16, "query x");
    _Static_assert(offsetof(SemuRenderPointerQuery, clamp) == 40, "query tail");
    _Static_assert(sizeof(SemuRenderPointerResult) == 56, "result size");
    _Static_assert(offsetof(SemuRenderPointerResult, frame_id) == 8, "result frame");
    _Static_assert(offsetof(SemuRenderPointerResult, surface_index) == 16,
      "result surface");
    _Static_assert(offsetof(SemuRenderPointerResult, native_y) == 52, "result tail");

    _Static_assert(sizeof(SemuRenderGetProc) == sizeof(void*), "get-proc pointer size");
    _Static_assert(sizeof(SemuRenderCurrentContext) == sizeof(void*),
      "context pointer size");
    _Static_assert(sizeof(SemuRenderMapPointer) == sizeof(void*),
      "map callback pointer size");
    _Static_assert(sizeof(SemuRenderFrameGl) == 232, "frame size");
    _Static_assert(offsetof(SemuRenderFrameGl, abi) == 0, "frame abi");
    _Static_assert(offsetof(SemuRenderFrameGl, struct_size) == 4, "frame size field");
    _Static_assert(offsetof(SemuRenderFrameGl, frame_id) == 8, "frame id");
    _Static_assert(offsetof(SemuRenderFrameGl, framebuffer) == 16, "framebuffer");
    _Static_assert(offsetof(SemuRenderFrameGl, color_buffer) == 20, "color buffer");
    _Static_assert(offsetof(SemuRenderFrameGl, framebuffer_width) == 24,
      "framebuffer width");
    _Static_assert(offsetof(SemuRenderFrameGl, framebuffer_height) == 28,
      "framebuffer height");
    _Static_assert(offsetof(SemuRenderFrameGl, presentation_aspect) == 32,
      "presentation aspect");
    _Static_assert(offsetof(SemuRenderFrameGl, layout_variant) == 36,
      "layout variant");
    _Static_assert(offsetof(SemuRenderFrameGl, surface_count) == 40,
      "surface count");
    _Static_assert(offsetof(SemuRenderFrameGl, surfaces) == 44, "surfaces");
    _Static_assert(offsetof(SemuRenderFrameGl, get_proc) == 112, "get-proc callback");
    _Static_assert(offsetof(SemuRenderFrameGl, current_context) == 120,
      "context callback");
    _Static_assert(offsetof(SemuRenderFrameGl, pointer_map) == 128, "pointer map");
    _Static_assert(offsetof(SemuRenderFrameGl, map_pointer) == 224,
      "map callback");

    struct BoundedFrame {
      unsigned char prefix[32];
      SemuRenderFrameGl frame;
      unsigned char suffix[32];
    };

    static void* missing_proc(const char* name) {
      (void)name;
      return NULL;
    }

    static void* no_context(void) {
      return NULL;
    }

    static int guarded_bytes_are(const unsigned char* bytes, size_t count,
                                 unsigned char expected) {
      for (size_t index = 0; index < count; index++) {
        if (bytes[index] != expected) return 0;
      }
      return 1;
    }

    static int reject_undersized_without_writes(void) {
      struct BoundedFrame bounded;
      unsigned char snapshot[sizeof(SemuRenderFrameGl)];
      memset(&bounded, 0xa5, sizeof(bounded));
      memset(&bounded.frame, 0, offsetof(SemuRenderFrameGl, pointer_map));
      bounded.frame.abi = SEMU_RENDER_ABI_GL;
      bounded.frame.struct_size = (unsigned int)
        offsetof(SemuRenderFrameGl, pointer_map);
      memcpy(snapshot, &bounded.frame, sizeof(snapshot));
      int status = semu_render_game_gl(&bounded.frame);
      return status == SEMU_RENDER_INVALID_FRAME
        && memcmp(snapshot, &bounded.frame, sizeof(snapshot)) == 0
        && guarded_bytes_are(bounded.prefix, sizeof(bounded.prefix), 0xa5)
        && guarded_bytes_are(bounded.suffix, sizeof(bounded.suffix), 0xa5);
    }

    static int exact_frame_publishes_callback_tail(void) {
      struct BoundedFrame bounded;
      memset(&bounded, 0, sizeof(bounded));
      memset(bounded.prefix, 0xa5, sizeof(bounded.prefix));
      memset(bounded.suffix, 0xa5, sizeof(bounded.suffix));
      SemuRenderFrameGl* frame = &bounded.frame;
      frame->abi = SEMU_RENDER_ABI_GL;
      frame->struct_size = sizeof(*frame);
      frame->frame_id = 1;
      frame->framebuffer_width = 1;
      frame->framebuffer_height = 1;
      frame->presentation_aspect = 1.0f;
      frame->layout_variant = SEMU_RENDER_LAYOUT_DEFAULT;
      frame->surface_count = 1;
      frame->surfaces[0].width = 1;
      frame->surfaces[0].height = 1;
      frame->surfaces[0].native_width = 1;
      frame->surfaces[0].native_height = 1;
      frame->surfaces[0].origin = SEMU_RENDER_ORIGIN_BOTTOM_LEFT;
      frame->get_proc = missing_proc;
      frame->current_context = no_context;
      int status = semu_render_game_gl(frame);
      if (status != SEMU_RENDER_NO_CONTEXT
          || frame->pointer_map.abi != SEMU_RENDER_ABI_GL
          || frame->pointer_map.struct_size != sizeof(SemuRenderPointerMap)
          || frame->map_pointer == NULL
          || !guarded_bytes_are(bounded.prefix, sizeof(bounded.prefix), 0xa5)
          || !guarded_bytes_are(bounded.suffix, sizeof(bounded.suffix), 0xa5)) {
        return 0;
      }
      SemuRenderPointerQuery query = {0};
      SemuRenderPointerResult result = {0};
      query.abi = SEMU_RENDER_ABI_GL;
      query.struct_size = sizeof(query);
      query.viewport_width = 1;
      query.viewport_height = 1;
      query.origin = SEMU_RENDER_ORIGIN_TOP_LEFT;
      query.surface_index = -1;
      return frame->map_pointer(&frame->pointer_map, &query, &result)
          == SEMU_RENDER_POINTER_UNAVAILABLE
        && result.abi == SEMU_RENDER_ABI_GL
        && result.struct_size == sizeof(result);
    }

    int main(void) {
      if (!reject_undersized_without_writes()) {
        fputs("undersized ABI-2 frame mutated caller storage\n", stderr);
        return 1;
      }
      if (!exact_frame_publishes_callback_tail()) {
        fputs("exact ABI-2 frame callback tail failed\n", stderr);
        return 2;
      }
      return 0;
    }
  '';
  libraryName = if stdenv.hostPlatform.isDarwin
    then "libsemurenderer.dylib"
    else "libsemurenderer.so";
in
stdenv.mkDerivation {
  pname = "semu-renderer";
  version = "2";
  src = rendererSource;

  dontConfigure = true;
  strictDeps = true;
  nativeBuildInputs = [ btrcpy ];
  buildInputs = [ librashader ];

  buildPhase = ''
    runHook preBuild
    cd src/generators/rendering/native

    btrcpy libsemurenderer.btrc -o semu_renderer.c \
      --strict-imports --no-cache --no-stdlib --no-dce

    warnings="-Wall -Wextra -Werror -Wno-unused-function"
    $CC -c semu_renderer.c -o semu_renderer.o \
      -std=c11 -O2 -fPIC $warnings \
      -DLIBRA_RUNTIME_OPENGL=1 \
      -DSTB_IMAGE_IMPLEMENTATION -DSTBI_ONLY_PNG \
      -I${librashader}/include -I.

    ${lib.optionalString stdenv.hostPlatform.isLinux ''
      cat > exports.map <<'MAP'
      {
        global:
          semu_render_game_gl;
          semu_render_post_ui_gl;
        local: *;
      };
      MAP
      $CC -shared -Wl,-z,defs \
        -Wl,-soname,libsemurenderer.so \
        -Wl,--version-script=exports.map \
        semu_renderer.o \
        -L${librashader}/lib -Wl,-rpath,${librashader}/lib \
        -lrashader -lm -o libsemurenderer.so

      btrcpy ../../input/linux_supervisor.btrc \
        -o semu_input_supervisor.c \
        --strict-imports --no-cache --no-stdlib --no-dce
      $CC semu_input_supervisor.c -o semu-input-supervisor \
        -std=c11 -O2 $warnings -D_GNU_SOURCE
      ./semu-input-supervisor --self-test
    ''}

    ${lib.optionalString stdenv.hostPlatform.isDarwin ''
      $CC -dynamiclib -Wl,-undefined,error \
        -Wl,-install_name,$out/lib/libsemurenderer.dylib \
        -Wl,-exported_symbol,_semu_render_game_gl \
        -Wl,-exported_symbol,_semu_render_post_ui_gl \
        semu_renderer.o \
        -L${librashader}/lib -Wl,-rpath,${librashader}/lib \
        -lrashader -lm -o libsemurenderer.dylib
    ''}
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$out/include" "$out/lib" "$out/share/semu"
    cp ${rendererHeader} "$out/include/semu_renderer.h"
    cp ${libraryName} "$out/lib/${libraryName}"

    ${lib.optionalString stdenv.hostPlatform.isLinux ''
      mkdir -p "$out/bin" "$out/lib/semu"
      cp semu-input-supervisor "$out/bin/semu-input-supervisor"
      ln -s ../../bin/semu-input-supervisor \
        "$out/lib/semu/semu-input-supervisor"
    ''}

    cat > "$out/share/semu/renderer-contract.json" <<'JSON'
    {
      "schema_version": 1,
      "abi": 2,
      "header": "include/semu_renderer.h",
      "library": "lib/${libraryName}",
      "exports": [
        "semu_render_game_gl",
        "semu_render_post_ui_gl"
      ],
      "game_phase": {
        "scope": "semantic_game_surfaces",
        "pipeline": [
          "shader",
          "bezel"
        ],
        "returns_before": "emulator_ui"
      },
      "post_ui_phase": {
        "game_treatment": false,
        "operations": [
          "semu_menu",
          "requested_evidence"
        ]
      }
    }
    JSON
    runHook postInstall
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    test -s "$out/include/semu_renderer.h"
    test -s "$out/lib/${libraryName}"
    grep -Fq '#define SEMU_RENDER_ABI_GL 2u' \
      "$out/include/semu_renderer.h"
    grep -Fq 'semu_render_game_gl' "$out/include/semu_renderer.h"
    grep -Fq 'semu_render_post_ui_gl' "$out/include/semu_renderer.h"
    grep -Fq '"scope": "semantic_game_surfaces"' \
      "$out/share/semu/renderer-contract.json"
    grep -Fq '"game_treatment": false' \
      "$out/share/semu/renderer-contract.json"
    $CC ${abiConformanceSource} -o renderer-abi-conformance \
      -std=c11 -Wall -Wextra -Werror \
      -I"$out/include" -L"$out/lib" -Wl,-rpath,"$out/lib" \
      -lsemurenderer
    ./renderer-abi-conformance
    ${lib.optionalString stdenv.hostPlatform.isLinux ''
      test -x "$out/bin/semu-input-supervisor"
      "$out/bin/semu-input-supervisor" --self-test
      nm -D --defined-only "$out/lib/libsemurenderer.so" \
        | awk '{ print $3 }' | sort > actual-exports
      printf '%s\n' semu_render_game_gl semu_render_post_ui_gl \
        | sort > expected-exports
      cmp expected-exports actual-exports
    ''}
    ${lib.optionalString stdenv.hostPlatform.isDarwin ''
      nm -gU "$out/lib/libsemurenderer.dylib" \
        | awk '{ print $3 }' | sed 's/^_//' | sort > actual-exports
      printf '%s\n' semu_render_game_gl semu_render_post_ui_gl \
        | sort > expected-exports
      cmp expected-exports actual-exports
    ''}
  '';

  passthru = {
    abi = 2;
    header = rendererHeader;
    inherit libraryName;
  };

  meta = {
    description = "Semu direct in-process OpenGL renderer";
    platforms = lib.platforms.linux ++ lib.platforms.darwin;
  };
}
