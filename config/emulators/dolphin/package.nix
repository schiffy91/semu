# dolphin.nix — Dolphin built from Semu's pinned source on every supported
# host. The source patch owns both the generated F13-F16 controller modes and
# the OpenGL calls into Semu's directly linked renderer.
#
# The source is Semu's own pin of the current stable tag; the build wiring
# (dependency set, cmake flags, the darwin Dolphin.app install) is inherited
# from nixpkgs dolphin-emu (a proven darwin source recipe), overridden onto
# this pin.
{
  lib,
  stdenv,
  fetchFromGitHub,
  dolphin-emu,
  semuRenderer,
}:

let
  packageContract = builtins.fromJSON (builtins.readFile ./package.json);
  rendering = (import ../../../packaging/nix/render-hook.nix { inherit lib; }) {
    emulatorDir = ./.;
  };
  sourceContract = packageContract.source;
  outputContract = packageContract.outputs;
  renderHookContract = rendering.hook;
  semuPatch = rendering.patch;
  semuPatchHash = rendering.patchHash;
  semuPatchText = rendering.patchText;
  controllerModes = packageContract.controller_modes;
  renderHookEnabled = lib.elem stdenv.hostPlatform.system renderHookContract.platforms;
  renderHook = renderHookContract // {
    enabled = renderHookEnabled;
  };
  buildContract = {
    schema_version = packageContract.schema_version;
    build_kind = packageContract.build_kind;
    version = packageContract.version;
    platforms = packageContract.platforms;
    graphics_apis = packageContract.graphics_apis;
    source_revision = sourceContract.revision;
    source_sha256 = sourceContract.sha256;
    patch_sha256 = semuPatchHash;
    controller_modes = controllerModes;
    render_hook = renderHook;
  };
  buildContractJson = builtins.toJSON buildContract;

  # ONE pinned source: tag 2603a (annotated tag peeled to its commit),
  # resolved 2026-07-02 via git ls-remote — pin the commit, not the movable
  # tag. Submodules carry the Externals/ tree upstream builds against, and
  # the postFetch COMMIT file feeds the recipe's preConfigure version stamp
  # (same trick as nixpkgs; the hash matches its tag fetch).
  source = fetchFromGitHub {
    inherit (sourceContract) owner repo;
    rev = sourceContract.revision;
    fetchSubmodules = sourceContract.fetch_submodules;
    leaveDotGit = true;
    postFetch = ''
      pushd $out
      git rev-parse HEAD 2>/dev/null >$out/COMMIT
      find $out -name .git -print0 | xargs -0 rm -rf
      popd
    '';
    hash = sourceContract.sha256;
  };

  patches = [ semuPatch ];

  sourceBuild =
    assert lib.assertMsg (
      packageContract.schema_version == 1
      && packageContract.id == "dolphin-emu"
      && packageContract.build_kind == "source"
      &&
        packageContract.platforms == [
          "x86_64-linux"
          "aarch64-darwin"
          "x86_64-darwin"
        ]
      && packageContract.graphics_apis == [ "opengl" ]
    ) "Dolphin's package contract must remain an exact OpenGL source build";
    assert lib.assertMsg (
      sourceContract.kind == "github"
      && builtins.match "^[0-9a-f]{40}$" sourceContract.revision != null
      && lib.hasPrefix "sha256-" sourceContract.sha256
      && sourceContract.fetch_submodules
    ) "Dolphin's source contract must be an immutable GitHub revision with submodules";
    assert lib.assertMsg (
      !packageContract.fallbacks.linux_host_executable
      && !packageContract.fallbacks.linux_flatpak
      && !packageContract.fallbacks.linux_prebuilt_binary
      && !packageContract.fallbacks.macos_host_executable
      && !packageContract.fallbacks.macos_prebuilt_binary
    ) "Dolphin's package contract must reject host, Flatpak, and prebuilt fallbacks";
    assert lib.assertMsg (
      renderHookContract.abi == 2
      && renderHookContract.linkage == "direct"
      &&
        renderHookContract.callbacks == [
          "get_proc_address"
          "current_context"
        ]
      && lib.hasInfix "semu_render_game_gl(frame);" semuPatchText
      && lib.hasInfix "semu_render_post_ui_gl(frame);" semuPatchText
      && lib.hasInfix "ActivateSemuControllerMode" semuPatchText
    ) "Dolphin's Semu patch must contain both direct renderer phases and controller modes";
    dolphin-emu.overrideAttrs (
      previous:
      let
        previousEnvironment = previous.env or { };
      in
      {
        pname = packageContract.id;
        version = packageContract.version;
        src = source;
        patches = (previous.patches or [ ]) ++ patches;
        patchFlags = (previous.patchFlags or [ "-p1" ]) ++ [ "--fuzz=0" ];
        cmakeFlags = (previous.cmakeFlags or [ ]) ++ [ "-DENABLE_TESTS=OFF" ];
        buildInputs = (previous.buildInputs or [ ]) ++ lib.optionals renderHookEnabled [ semuRenderer ];
        postPatch =
          (previous.postPatch or "")
          + lib.optionalString renderHookEnabled ''
            test -s ${semuRenderer}/include/${renderHookContract.header}
            test "$(grep -cF 'semu_render_game_gl(frame);' \
              Source/Core/VideoBackends/OGL/OGLGfx.cpp)" -eq 1
            test "$(grep -cF 'semu_render_post_ui_gl(frame);' \
              Source/Core/VideoBackends/OGL/OGLGfx.cpp)" -eq 1
            grep -Fq 'semu_frame.presentation_aspect = CalculateDrawAspectRatio(false);' \
              Source/Core/VideoCommon/Present.cpp
            grep -Fq 'g_widescreen->IsGameWidescreen()' \
              Source/Core/VideoCommon/Present.cpp
            grep -Fq 'SEMU_RENDER_LAYOUT_VARIANT_B : SEMU_RENDER_LAYOUT_DEFAULT' \
              Source/Core/VideoCommon/Present.cpp
            grep -Fq 'semu_frame.framebuffer_width = m_backbuffer_width;' \
              Source/Core/VideoCommon/Present.cpp
            grep -Fq 'semu_frame.framebuffer_height = m_backbuffer_height;' \
              Source/Core/VideoCommon/Present.cpp
            grep -Fq 'semu_frame.surfaces[0].width = render_target_rc.GetWidth();' \
              Source/Core/VideoCommon/Present.cpp
            grep -Fq 'semu_frame.surfaces[0].height = render_target_rc.GetHeight();' \
              Source/Core/VideoCommon/Present.cpp
            grep -Fq 'semu_frame.surfaces[0].native_width = static_cast<int>(m_last_xfb_width);' \
              Source/Core/VideoCommon/Present.cpp
            grep -Fq 'semu_frame.surfaces[0].native_height = static_cast<int>(m_last_xfb_height);' \
              Source/Core/VideoCommon/Present.cpp
            xfb_line="$(grep -nF 'RenderXFBToScreen(render_target_rc' \
              Source/Core/VideoCommon/Present.cpp | cut -d: -f1)"
            game_line="$(grep -nF 'g_gfx->RenderSemuGameFrame(&semu_frame);' \
              Source/Core/VideoCommon/Present.cpp | cut -d: -f1)"
            finalize_line="$(grep -nF 'm_onscreen_ui->Finalize();' \
              Source/Core/VideoCommon/Present.cpp | cut -d: -f1)"
            draw_ui_line="$(grep -nF 'm_onscreen_ui->DrawImGui();' \
              Source/Core/VideoCommon/Present.cpp | cut -d: -f1)"
            post_line="$(grep -nF 'g_gfx->RenderSemuPostUiFrame(&semu_frame);' \
              Source/Core/VideoCommon/Present.cpp | cut -d: -f1)"
            present_line="$(grep -nF 'g_gfx->PresentBackbuffer();' \
              Source/Core/VideoCommon/Present.cpp | \
              awk -F: -v post="$post_line" '$1 > post { print $1; exit }')"
            test "$xfb_line" -lt "$game_line"
            test "$game_line" -lt "$finalize_line"
            test "$finalize_line" -lt "$draw_ui_line"
            test "$draw_ui_line" -lt "$post_line"
            test "$present_line" -eq "$((post_line + 2))"
          '';

        env =
          previousEnvironment
          // lib.optionalAttrs renderHookEnabled {
            NIX_CFLAGS_COMPILE =
              (previousEnvironment.NIX_CFLAGS_COMPILE or "")
              + " -D${renderHookContract.compile_define} -I${semuRenderer}/include";
            NIX_LDFLAGS =
              (previousEnvironment.NIX_LDFLAGS or "")
              + " -L${semuRenderer}/lib -rpath ${semuRenderer}/lib"
              + " -l${renderHookContract.library}";
          };

        postInstall = (previous.postInstall or "") + ''
          mkdir -p "$out/share/semu"
          printf '%s\n' ${lib.escapeShellArg buildContractJson} \
            > "$out/${outputContract.build_contract}"
        '';

        doInstallCheck = true;
        installCheckPhase = ''
          runHook preInstallCheck
          contract="$out/${outputContract.build_contract}"
          test -s "$contract"
          test "$(cat "$contract")" = ${lib.escapeShellArg buildContractJson}
          ${lib.optionalString stdenv.hostPlatform.isLinux (
            if renderHookEnabled then
              ''
                renderer_launcher="$out/${outputContract.linux_main_program}"
                test -x "$renderer_launcher"
                renderer_binary="$(dirname "$renderer_launcher")/.$(basename "$renderer_launcher")-wrapped"
                test -x "$renderer_binary"
                grep -aFq semu_render_game_gl "$renderer_binary"
                grep -aFq semu_render_post_ui_gl "$renderer_binary"
                readelf -d "$renderer_binary" > "$NIX_BUILD_TOP/dolphin.dynamic"
                grep -Fq 'lib${renderHookContract.library}.so' \
                  "$NIX_BUILD_TOP/dolphin.dynamic"
                grep -Fq '${semuRenderer}/lib' \
                  "$NIX_BUILD_TOP/dolphin.dynamic"
              ''
            else
              ''
                test -x "$out/${outputContract.linux_main_program}"
              ''
          )}
          ${lib.optionalString stdenv.hostPlatform.isDarwin ''
            renderer_launcher="$out/${outputContract.macos_main_program}"
            test -x "$renderer_launcher"
            ${lib.optionalString renderHookEnabled ''
              renderer_binary="$(dirname "$renderer_launcher")/.$(basename "$renderer_launcher")-wrapped"
              test -x "$renderer_binary"
              grep -aFq semu_render_game_gl "$renderer_binary"
              grep -aFq semu_render_post_ui_gl "$renderer_binary"
              otool -L "$renderer_binary" > "$TMPDIR/dolphin-libraries.txt"
              otool -l "$renderer_binary" > "$TMPDIR/dolphin-load-commands.txt"
              grep -Fq 'lib${renderHookContract.library}.dylib' \
                "$TMPDIR/dolphin-libraries.txt"
              grep -Fq '${semuRenderer}/lib' \
                "$TMPDIR/dolphin-load-commands.txt"
            ''}
          ''}
          runHook postInstallCheck
        '';

        passthru = (previous.passthru or { }) // {
          semuControllerModes = controllerModes;
          semuRenderHook = renderHook;
          semuBuildContract = buildContract;
        };
      }
    );
in
sourceBuild
