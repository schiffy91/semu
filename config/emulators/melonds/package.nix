# package.json owns melonDS package identity and outputs; rendering.json owns
# the renderer ABI and hook boundaries interpreted by this source recipe.
{
  lib,
  stdenv,
  faad2,
  fetchFromGitHub,
  melonds,
  semuRenderer,
}:

let
  packageContract = builtins.fromJSON (builtins.readFile ./package.json);
  rendering = (import ../../../packaging/nix/render-hook.nix { inherit lib; }) {
    emulatorDir = ./.;
  };
  sourceContract = packageContract.source;
  outputContract = packageContract.outputs;
  renderHook = rendering.hook;
  pointerMapping = renderHook.pointer_mapping;
  gamePhase = rendering.gamePhase;
  postUiPhase = rendering.postUiPhase;
  fallbacks = packageContract.fallbacks;
  nativeActions = packageContract.native_actions;
  metadata = packageContract.metadata;

  source = fetchFromGitHub {
    inherit (sourceContract) owner repo;
    rev = sourceContract.revision;
    hash = sourceContract.sha256;
  };

  semuPatch = rendering.patch;
  semuPatchText = rendering.patchText;
  semuPatchHash = rendering.patchHash;

  buildContract = {
    schema_version = packageContract.schema_version;
    build_kind = packageContract.build_kind;
    version = packageContract.version;
    platforms = packageContract.platforms;
    graphics_apis = packageContract.graphics_apis;
    source_kind = sourceContract.kind;
    source_owner = sourceContract.owner;
    source_repo = sourceContract.repo;
    source_revision = sourceContract.revision;
    source_url = sourceContract.url;
    source_sha256 = sourceContract.sha256;
    patch_sha256 = semuPatchHash;
    native_actions = nativeActions;
    executable = outputContract.main_program;
    linked_executable = outputContract.linked_program;
    render_hook = renderHook;
  };
  buildContractJson = builtins.toJSON buildContract;

  sourceBuild =
    assert lib.assertMsg (
      packageContract.schema_version == 1
      && packageContract.id == "melonds"
      && packageContract.build_kind == "source"
      && packageContract.platforms == [ "x86_64-linux" ]
      && packageContract.graphics_apis == [ "opengl" ]
      &&
        builtins.attrNames nativeActions == [
          "state.load"
          "state.save"
        ]
      && nativeActions."state.load".chord == "Ctrl+A"
      && nativeActions."state.load".slot == 1
      && nativeActions."state.save".chord == "Ctrl+S"
      && nativeActions."state.save".slot == 1
    ) "melonDS's package contract must remain an x86_64-linux OpenGL source build";
    assert lib.assertMsg (
      sourceContract.kind == "github"
      && builtins.match "^[0-9a-f]{40}$" sourceContract.revision != null
      &&
        sourceContract.url
        == "https://github.com/${sourceContract.owner}/${sourceContract.repo}/archive/${sourceContract.revision}.tar.gz"
      && lib.hasPrefix "sha256-" sourceContract.sha256
    ) "melonDS's source contract must remain an immutable GitHub revision";
    assert lib.assertMsg (
      !fallbacks.host_executable
      && !fallbacks.flatpak
      && !fallbacks.libretro
      && !fallbacks.prebuilt_binary
    ) "melonDS's package contract must forbid every fallback";
    assert lib.assertMsg (
      renderHook.callbacks == [
        "get_proc_address"
        "current_context"
      ]
      && renderHook.framebuffer == "default_opengl_draw_framebuffer"
      && renderHook.origin == "top_left"
      && renderHook.orientation == "display_upright"
      && renderHook.rotation == 0
      && lib.hasInfix "semu_render_game_gl(&semuFrame);" semuPatchText
      && lib.hasInfix "semu_render_post_ui_gl(&semuFrame);" semuPatchText
      && lib.hasInfix "surface.rotation = 0;" semuPatchText
      && !lib.hasInfix "rotation * 90" semuPatchText
    ) "melonDS's source patch must carry the direct two-phase renderer ABI";
    assert lib.assertMsg (
      pointerMapping.map == "frame.pointer_map"
      && pointerMapping.callback == "frame.map_pointer"
      && pointerMapping.snapshot == "last_successful_post_ui_frame"
      && pointerMapping.surface_index == 1
      && pointerMapping.surface_id == "bottom"
      && pointerMapping.viewport == "qt_logical_widget"
      && pointerMapping.origin == "top_left"
      && pointerMapping.press_clamp == false
      && pointerMapping.captured_motion_clamp == true
      &&
        pointerMapping.events == [
          "mouse_press"
          "mouse_move"
          "tablet"
          "touch"
        ]
      && pointerMapping.consumer == "EmuInstance::touchScreen"
      && pointerMapping.coordinates == "abi_native_pixels"
    ) "melonDS's touch policy must map Qt input through ABI 2 bottom surface 1";
    melonds.overrideAttrs (
      previous:
      let
        previousEnvironment = previous.env or { };
      in
      {
        pname = packageContract.id;
        version = packageContract.version;
        src = source;
        patches = (previous.patches or [ ]) ++ [ semuPatch ];
        patchFlags = (previous.patchFlags or [ "-p1" ]) ++ [ "--fuzz=0" ];
        buildInputs = lib.unique (
          (previous.buildInputs or [ ])
          ++ [
            faad2
            semuRenderer
          ]
        );

        postPatch = (previous.postPatch or "") + ''
          test -s ${semuRenderer}/include/${renderHook.header}
          screen=${lib.escapeShellArg gamePhase.source}
          screen_header=src/frontend/qt_sdl/Screen.h
          instance=src/frontend/qt_sdl/EmuInstance.cpp
          window=src/frontend/qt_sdl/Window.cpp
          grep -Fq 'frame.surface_count = valid && seen[0] && seen[1] ? 2 : 0;' "$screen"
          grep -Fq 'frame.surfaces[screenKindValue]' "$screen"
          grep -Fq 'SEMU_RENDER_ORIGIN_TOP_LEFT' "$screen"
          grep -Fq 'surface.rotation = 0;' "$screen"
          ! grep -Fq 'rotation * 90' "$screen"
          grep -Fq 'SemuRenderPointerMap semuPointerMap{};' "$screen_header"
          grep -Fq 'SemuRenderMapPointer semuMapPointer = nullptr;' "$screen_header"
          grep -Fq 'semuPointerMap = frame->pointer_map;' "$screen"
          grep -Fq 'semuMapPointer = frame->map_pointer;' "$screen"
          grep -Fq 'query.frame_id = pointerMap.frame_id;' "$screen"
          grep -Fq 'query.origin = SEMU_RENDER_ORIGIN_TOP_LEFT;' "$screen"
          grep -Fq 'query.surface_index = 1;' "$screen"
          grep -Fq 'query.clamp = capture ? 1 : 0;' "$screen"
          grep -Fq 'nativeX = static_cast<int>(std::lround(result.native_x));' "$screen"
          grep -Fq 'nativeY = static_cast<int>(std::lround(result.native_y));' "$screen"
          test "$(grep -Fc 'mapSemuTouch(x, y, false, x, y)' "$screen")" -eq 1
          test "$(grep -Fc 'mapSemuTouch(x, y, true, x, y)' "$screen")" -eq 1
          test "$(grep -Fc '&& mapSemuTouch(x, y, capture, x, y)' "$screen")" -eq 2
          test "$(grep -Fc 'emuInstance->touchScreen(x, y);' "$screen")" -eq 4
          ! grep -Fq 'layout.GetTouchCoords' "$screen"

          draw_line="$(grep -nF 'glDrawArrays(GL_TRIANGLES, screenKind[i] == 0' \
            "$screen" | cut -d: -f1)"
          game_line="$(grep -nF 'semu_render_game_gl(&semuFrame);' \
            "$screen" | cut -d: -f1)"
          osd_line="$(grep -nF 'osdUpdate();' "$screen" \
            | tail -n 1 | cut -d: -f1)"
          post_line="$(grep -nF 'semu_render_post_ui_gl(&semuFrame);' \
            "$screen" | cut -d: -f1)"
          map_store_line="$(grep -nF 'storeSemuPointerMap(semuGameApplied && semuPostUiApplied' "$screen" | cut -d: -f1)"
          swap_line="$(grep -nF 'glContext->SwapBuffers();' "$screen" | cut -d: -f1)"
          test -n "$draw_line" -a -n "$game_line" -a -n "$osd_line"
          test "$draw_line" -lt "$game_line"
          test "$game_line" -lt "$osd_line"
          test "$osd_line" -lt "$post_line"
          test "$post_line" -lt "$map_store_line"
          test "$map_store_line" -lt "$swap_line"
          grep -Fq 'Platform::GetLocalFilePath(result)' "$instance"
          grep -Fq 'externalSavename = getAssetPath(false, "", ".sav")' "$instance"
          grep -Fq 'Platform::OpenFile(externalSavename, FileMode::Read)' "$instance"
          grep -Fq 'GetLocalFilePath(globalCfg.GetString("DS.FirmwarePath"))' "$instance"
          grep -Fq ${lib.escapeShellArg nativeActions."state.save".source_marker} "$window"
          grep -Fq ${lib.escapeShellArg nativeActions."state.load".source_marker} "$window"
          grep -Fq ${lib.escapeShellArg nativeActions."state.save".source_consumer_marker} "$window"
          grep -Fq ${lib.escapeShellArg nativeActions."state.load".source_consumer_marker} "$window"
          grep -Fq 'actSaveState[i]->setShortcut(QKeySequence(Qt::ShiftModifier | (Qt::Key_F1 + i - 1)));' "$window"
          grep -Fq 'actLoadState[i]->setShortcut(QKeySequence(Qt::Key_F1 + i - 1));' "$window"
        '';

        preConfigure = (previous.preConfigure or "") + ''
          export NIX_CFLAGS_COMPILE="$NIX_CFLAGS_COMPILE \
            -ffile-prefix-map=$NIX_BUILD_TOP=/build"
        '';

        cmakeFlags = (previous.cmakeFlags or [ ]) ++ [
          (lib.cmakeBool "USE_QT6" true)
          (lib.cmakeBool "ENABLE_OGLRENDERER" true)
        ];

        env = previousEnvironment // {
          NIX_CFLAGS_COMPILE = lib.concatStringsSep " " [
            (previousEnvironment.NIX_CFLAGS_COMPILE or "")
            "-I${semuRenderer}/include"
          ];
          NIX_LDFLAGS =
            (previousEnvironment.NIX_LDFLAGS or "")
            + " -L${semuRenderer}/lib -rpath ${semuRenderer}/lib"
            + " -l${renderHook.library}";
          MELONDS_GIT_HASH = sourceContract.revision;
          MELONDS_GIT_BRANCH = "semu-pinned";
          MELONDS_BUILD_PROVIDER = "Semu";
        };

        postInstall = (previous.postInstall or "") + ''
          mkdir -p "$out/share/semu"
          printf '%s\n' ${lib.escapeShellArg buildContractJson} \
            > "$out/${outputContract.build_contract}"
        '';

        doInstallCheck = true;
        installCheckPhase = ''
          runHook preInstallCheck
          test -x "$out/${outputContract.main_program}"
          test -x "$out/${outputContract.linked_program}"
          contract="$out/${outputContract.build_contract}"
          test -s "$contract"
          test "$(cat "$contract")" = ${lib.escapeShellArg buildContractJson}
          binary="$out/${outputContract.linked_program}"
          grep -aFq '${gamePhase.symbol}' "$binary"
          grep -aFq '${postUiPhase.symbol}' "$binary"
          dynamic_section="$(readelf -d "$binary")"
          grep -Fq 'lib${renderHook.library}.so' <<<"$dynamic_section"
          grep -Fq '${semuRenderer}/lib' <<<"$dynamic_section"
          runHook postInstallCheck
        '';

        passthru = (previous.passthru or { }) // {
          semuBuildContract = buildContract;
          semuRenderHook = renderHook;
        };

        meta = (previous.meta or { }) // {
          inherit (metadata) description homepage;
          license = [ lib.licenses.${metadata.license} ];
          mainProgram = outputContract.main_program_name;
          platforms = packageContract.platforms;
        };
      }
    );
in
assert lib.assertMsg (lib.elem stdenv.hostPlatform.system packageContract.platforms)
  "Semu melonDS supports only the package.json platform set";
sourceBuild
