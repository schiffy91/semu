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
