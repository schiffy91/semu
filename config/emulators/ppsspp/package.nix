# PPSSPP is an exact upstream source build. package.json owns package identity;
# rendering.json owns the renderer ABI and hook boundaries.
{
  lib,
  stdenv,
  fetchFromGitHub,
  ppsspp,
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
  fallbacks = packageContract.fallbacks;
  metadata = packageContract.metadata;

  exactSource = fetchFromGitHub {
    inherit (sourceContract) owner repo;
    rev = sourceContract.revision;
    fetchSubmodules = sourceContract.fetch_submodules;
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
    source_sha256 = sourceContract.sha256;
    source_fetch_submodules = sourceContract.fetch_submodules;
    patch_sha256 = semuPatchHash;
    executable = outputContract.main_program;
    patched_binary = outputContract.patched_binary;
    render_hook = renderHook;
  };
  buildContractJson = builtins.toJSON buildContract;

  basePackage = ppsspp.override {
    enableQt = false;
    enableVulkan = false;
    forceWayland = false;
  };

  sourceBuild =
    assert lib.assertMsg (
      packageContract.schema_version == 1
      && packageContract.id == "ppsspp"
      && packageContract.build_kind == "source"
      && packageContract.platforms == [ "x86_64-linux" ]
      && packageContract.graphics_apis == [ "opengl" ]
    ) "PPSSPP's package contract must remain an x86_64-linux OpenGL source build";
    assert lib.assertMsg (
      sourceContract.kind == "github_revision_with_submodules"
      && builtins.match "^[0-9a-f]{40}$" sourceContract.revision != null
      && lib.hasPrefix "sha256-" sourceContract.sha256
    ) "PPSSPP's source contract must be an immutable GitHub revision";
    assert lib.assertMsg (
      sourceContract.tag == "v${packageContract.version}"
    ) "PPSSPP's source tag and package version must match";
    assert lib.assertMsg sourceContract.fetch_submodules
      "PPSSPP's exact source must include upstream submodules";
    assert lib.assertMsg (
      !fallbacks.host_executable
      && !fallbacks.flatpak
      && !fallbacks.libretro
      && !fallbacks.prebuilt_binary
    ) "PPSSPP's package contract must reject every runtime fallback";
    assert lib.assertMsg (
      renderHook.callbacks == [
        "get_proc_address"
        "current_context"
      ]
      && renderHook.framebuffer == "default_opengl_draw_framebuffer"
      && renderHook.origin == "top_left"
      && renderHook.orientation == "upright"
      && renderHook.rotation == 0
      && lib.hasInfix "semu_render_game_gl(&step.semuRenderFrame);" semuPatchText
      && lib.hasInfix "semu_render_post_ui_gl(&step.semuRenderFrame);" semuPatchText
      && lib.hasInfix "frame.surfaces[0].rotation = 0;" semuPatchText
    ) "PPSSPP's source patch must carry the direct two-phase renderer ABI";
    basePackage.overrideAttrs (
      previous:
      let
        previousEnvironment = previous.env or { };
      in
      {
        pname = packageContract.id;
        version = packageContract.version;
        src = exactSource;
        patches = (previous.patches or [ ]) ++ [ semuPatch ];
        patchFlags = (previous.patchFlags or [ "-p1" ]) ++ [ "--fuzz=0" ];
        buildInputs = (previous.buildInputs or [ ]) ++ [ semuRenderer ];

        postPatch = (previous.postPatch or "") + ''
          grep -Fq ${lib.escapeShellArg sourceContract.revision} git-version.cmake

          test -s ${semuRenderer}/include/${renderHook.header}
          game_source=${lib.escapeShellArg gamePhase.source}
          post_ui_source=${lib.escapeShellArg postUiPhase.source}
          queue_source=Common/GPU/OpenGL/GLQueueRunner.cpp
          game_draw_line="$(grep -nF 'draw_->Draw(4, 0);' "$game_source" | tail -n 1 | cut -d: -f1)"
          game_queue_line="$(grep -nF 'renderManager->QueueSemuGameRender(frame);' "$game_source" | cut -d: -f1)"
          release_line="$(grep -nF 'DoRelease(srcFramebuffer_);' "$game_source" | tail -n 1 | cut -d: -f1)"
          post_step_line="$(awk '/GLRStepType::SEMU_RENDER_POST_UI/ { print NR; exit }' "$post_ui_source")"
          present_task_line="$(grep -nF 'GLRRenderThreadTask *presentTask = new GLRRenderThreadTask(GLRRunType::PRESENT);' "$post_ui_source" | cut -d: -f1)"
          test "$game_draw_line" -lt "$game_queue_line"
          test "$game_queue_line" -lt "$release_line"
          test "$post_step_line" -lt "$present_task_line"
          test "$(grep -cF 'semu_render_game_gl(&step.semuRenderFrame);' "$queue_source")" -eq 1
          test "$(grep -cF 'semu_render_post_ui_gl(&step.semuRenderFrame);' "$queue_source")" -eq 1
          grep -Fq 'frame.surfaces[0].native_width = 480;' "$game_source"
          grep -Fq 'frame.surfaces[0].native_height = 272;' "$game_source"
          grep -Fq 'frame.surfaces[0].rotation = 0;' "$game_source"
        '';

        cmakeFlags = (previous.cmakeFlags or [ ]) ++ [
          (lib.cmakeBool "USE_DISCORD" false)
          (lib.cmakeBool "UNITTEST" false)
          (lib.cmakeBool "USE_WAYLAND_WSI" false)
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
          test -x "$out/${outputContract.patched_binary}"
          contract="$out/${outputContract.build_contract}"
          test -s "$contract"
          test "$(cat "$contract")" = ${lib.escapeShellArg buildContractJson}
          binary="$out/${outputContract.patched_binary}"
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
          license = lib.licenses.${metadata.license};
          mainProgram = outputContract.main_program_name;
          platforms = packageContract.platforms;
        };
      }
    );
in
assert lib.assertMsg (lib.elem stdenv.hostPlatform.system packageContract.platforms)
  "Semu PPSSPP supports only the package.json platform set";
sourceBuild
