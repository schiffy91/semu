# Azahar is an exact upstream source build. package.json owns package identity;
# rendering.json owns the renderer ABI and hook boundaries.
{
  lib,
  stdenv,
  fetchurl,
  azahar,
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
  gamePhase = rendering.gamePhase;
  postUiPhase = rendering.postUiPhase;
  pointerMapping = rendering.contract.integration.pointer_mapping;
  surfaces = rendering.contract.surfaces;
  fallbacks = packageContract.fallbacks;
  metadata = packageContract.metadata;

  sourceArchive = fetchurl {
    inherit (sourceContract) url;
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
    source_tag = sourceContract.tag;
    source_revision = sourceContract.revision;
    source_url = sourceContract.url;
    source_sha256 = sourceContract.sha256;
    patch_sha256 = semuPatchHash;
    executable = outputContract.main_program;
    linked_executable = outputContract.linked_program;
    render_hook = renderHook;
  };
  buildContractJson = builtins.toJSON buildContract;

  sourceBuild =
    assert lib.assertMsg (
      packageContract.schema_version == 1
      && packageContract.id == "azahar"
      && packageContract.build_kind == "source"
      && packageContract.platforms == [ "x86_64-linux" ]
      && packageContract.graphics_apis == [ "opengl" ]
    ) "Azahar's package contract must remain an x86_64-linux OpenGL source build";
    assert lib.assertMsg (
      sourceContract.kind == "upstream_release_archive"
      && builtins.match "^[0-9a-f]{40}$" sourceContract.revision != null
      && lib.hasPrefix "https://" sourceContract.url
      && lib.hasPrefix "sha256-" sourceContract.sha256
    ) "Azahar's source contract must be an immutable upstream release archive";
    assert lib.assertMsg (
      sourceContract.tag == packageContract.version
    ) "Azahar's source tag and package version must match";
    assert lib.assertMsg (
      !fallbacks.host_executable
      && !fallbacks.flatpak
      && !fallbacks.libretro
      && !fallbacks.prebuilt_binary
    ) "Azahar's package contract must reject every runtime fallback";
    assert lib.assertMsg (
      renderHook.callbacks == [
        "get_proc_address"
        "current_context"
      ]
      && renderHook.framebuffer == "default_opengl_draw_framebuffer"
      && renderHook.origin == "top_left"
      && renderHook.orientation == "display_upright"
      && renderHook.rotation == 0
      && lib.hasInfix "semu_render_game_gl(&semu_render_frame);" semuPatchText
      && lib.hasInfix "semu_render_post_ui_gl(frame);" semuPatchText
      && lib.hasInfix "surface.rotation = 0;" semuPatchText
      && !lib.hasInfix "layout.is_rotated ?" semuPatchText
    ) "Azahar's source patch must carry the direct two-phase renderer ABI";
    assert lib.assertMsg (
      builtins.length surfaces == 2
      && pointerMapping.map == "frame.pointer_map"
      && pointerMapping.callback == "frame.map_pointer"
      && pointerMapping.snapshot == "copy_by_value_after_game_phase"
      && pointerMapping.surface_id == (builtins.elemAt surfaces 1).id
      && pointerMapping.surface_index == 1
      && pointerMapping.viewport == "qt_logical_widget"
      && pointerMapping.origin == "top_left"
      && pointerMapping.press_clamp == false
      && pointerMapping.captured_motion_clamp == true
      &&
        pointerMapping.events == [
          "mouse_press"
          "mouse_move"
          "touch_begin"
          "touch_update"
        ]
      && pointerMapping.consumer == "GRenderWindow"
      && pointerMapping.coordinates == "abi_native_pixels"
      && pointerMapping.delivery.capture == "successful_press"
      && pointerMapping.delivery.press == "TouchPressedNative"
      && pointerMapping.delivery.captured_motion == "TouchMovedNative"
      && pointerMapping.delivery.legacy_framebuffer_transform == false
      && lib.hasInfix "semu_pointer_state->map = frame->pointer_map;" semuPatchText
      && lib.hasInfix "semu_pointer_state->map_pointer = frame->map_pointer;" semuPatchText
      && lib.hasInfix "query.surface_index = 1;" semuPatchText
      && lib.hasInfix "query.clamp = clamp ? 1 : 0;" semuPatchText
      && lib.hasInfix "TouchPressedNative(" semuPatchText
      && lib.hasInfix "TouchMovedNative(" semuPatchText
    ) "Azahar's source patch must consume the ABI 2 bottom-surface pointer map";
    azahar.overrideAttrs (
      previous:
      let
        previousEnvironment = previous.env or { };
      in
      {
        pname = packageContract.id;
        version = packageContract.version;
        src = sourceArchive;
        patches = (previous.patches or [ ]) ++ [ semuPatch ];
        patchFlags = (previous.patchFlags or [ "-p1" ]) ++ [ "--fuzz=0" ];
        buildInputs = (previous.buildInputs or [ ]) ++ [ semuRenderer ];

        postPatch = (previous.postPatch or "") + ''
          test "$(tr -d '\r\n' < ${lib.escapeShellArg sourceContract.identity_files.tag})" = \
            ${lib.escapeShellArg sourceContract.tag}
          test "$(tr -d '\r\n' < ${lib.escapeShellArg sourceContract.identity_files.revision})" = \
            ${lib.escapeShellArg sourceContract.revision}

          test -s ${semuRenderer}/include/${renderHook.header}
          game_source=${lib.escapeShellArg gamePhase.source}
          post_ui_source=${lib.escapeShellArg postUiPhase.source}
          input_header=src/core/frontend/emu_window.h
          input_source=src/core/frontend/emu_window.cpp
          blit_reset_line="$(grep -nF 'glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);' "$game_source" | tail -n 1 | cut -d: -f1)"
          game_line="$(grep -nF 'semu_render_game_gl(&semu_render_frame);' "$game_source" | cut -d: -f1)"
          try_present_line="$(grep -nF 'system.GPU().Renderer().TryPresent(100, is_secondary);' "$post_ui_source" | cut -d: -f1)"
          publish_line="$(grep -nF 'render_window.PublishSemuPointerFrame(frame);' "$post_ui_source" | cut -d: -f1)"
          post_line="$(grep -nF 'semu_render_post_ui_gl(frame);' "$post_ui_source" | cut -d: -f1)"
          swap_line="$(grep -nF 'context->SwapBuffers();' "$post_ui_source" | cut -d: -f1)"
          test "$blit_reset_line" -lt "$game_line"
          test "$try_present_line" -lt "$publish_line"
          test "$publish_line" -lt "$post_line"
          test "$post_line" -lt "$swap_line"
          grep -Fq 'Core::kScreenTopWidth, Core::kScreenTopHeight' "$game_source"
          grep -Fq 'Core::kScreenBottomWidth, Core::kScreenBottomHeight' "$game_source"
          grep -Fq 'surface.rotation = 0;' "$game_source"
          grep -Fq 'SEMU_RENDER_ORIGIN_TOP_LEFT' "$game_source"
          grep -Fq 'semu_pointer_state->map = frame->pointer_map;' "$post_ui_source"
          grep -Fq 'semu_pointer_state->map_pointer = frame->map_pointer;' "$post_ui_source"
          grep -Fq 'query.frame_id = pointer_map.frame_id;' "$post_ui_source"
          grep -Fq 'query.surface_index = 1;' "$post_ui_source"
          grep -Fq 'query.clamp = clamp ? 1 : 0;' "$post_ui_source"
          test "$(grep -Fc 'ScaleTouch(pos, true)' "$post_ui_source")" -eq 2
          grep -Fq 'ScaleTouch(pos, false)' "$post_ui_source"
          grep -Fq 'ScaleTouch(event->points().first().position(), false)' "$post_ui_source"
          grep -Fq 'TouchPressedNative(' "$input_header"
          grep -Fq 'TouchMovedNative(' "$input_header"
          native_start="$(grep -nF 'bool EmuWindow::SetTouchNative' "$input_source" | cut -d: -f1)"
          native_end="$(grep -nF 'bool EmuWindow::TouchPressedNative' "$input_source" | cut -d: -f1)"
          test "$native_start" -lt "$native_end"
          native_body="$(sed -n "''${native_start},''${native_end}p" "$input_source")"
          grep -Fq 'touch_state->touch_x = native_x' <<<"$native_body"
          grep -Fq 'touch_state->touch_y = native_y' <<<"$native_body"
          ! grep -Fq 'framebuffer_layout' <<<"$native_body"
          ! grep -Fq 'is_rotated' <<<"$native_body"
          ! grep -Fq 'TouchPressed(' <<<"$native_body"
        '';

        cmakeFlags = (previous.cmakeFlags or [ ]) ++ [
          (lib.cmakeBool "ENABLE_OPENGL" true)
          (lib.cmakeBool "ENABLE_VULKAN" false)
          (lib.cmakeBool "ENABLE_QT_UPDATE_CHECKER" false)
          (lib.cmakeBool "USE_DISCORD_PRESENCE" false)
          (lib.cmakeBool "ENABLE_TESTS" false)
          (lib.cmakeBool "ENABLE_ROOM" false)
          (lib.cmakeBool "ENABLE_ROOM_STANDALONE" false)
          (lib.cmakeBool "ENABLE_WEB_SERVICE" false)
          (lib.cmakeBool "ENABLE_SCRIPTING" false)
        ];

        env = previousEnvironment // {
          NIX_CFLAGS_COMPILE = (previousEnvironment.NIX_CFLAGS_COMPILE or "") + " -I${semuRenderer}/include";
          NIX_LDFLAGS =
            (previousEnvironment.NIX_LDFLAGS or "")
            + " -L${semuRenderer}/lib -rpath ${semuRenderer}/lib"
            + " -l${renderHook.library}";
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
          renderer_binary="$out/${outputContract.linked_program}"
          grep -aFq '${gamePhase.symbol}' "$renderer_binary"
          grep -aFq '${postUiPhase.symbol}' "$renderer_binary"
          dynamic_section="$(readelf -d "$renderer_binary")"
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
          license = lib.licenses.${metadata.license};
          mainProgram = outputContract.main_program_name;
          platforms = packageContract.platforms;
        };
      }
    );
in
assert lib.assertMsg (lib.elem stdenv.hostPlatform.system packageContract.platforms)
  "Semu Azahar supports only the package.json platform set";
sourceBuild
