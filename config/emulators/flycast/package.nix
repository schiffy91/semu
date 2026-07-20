# Flycast is an exact upstream source build. package.json owns package identity;
# rendering.json owns the renderer ABI and hook boundaries.
{
  lib,
  stdenv,
  fetchFromGitHub,
  flycast,
  semuRenderer,
  systemdLibs,
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

  source = fetchFromGitHub {
    inherit (sourceContract) owner repo;
    rev = sourceContract.revision;
    hash = sourceContract.sha256;
    fetchSubmodules = sourceContract.fetch_submodules;
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
    linked_executable = outputContract.linked_program;
    render_hook = renderHook;
  };
  buildContractJson = builtins.toJSON buildContract;

  sourceBuild =
    assert lib.assertMsg (
      packageContract.schema_version == 1
      && packageContract.id == "flycast"
      && packageContract.build_kind == "source"
      && packageContract.platforms == [ "x86_64-linux" ]
      && packageContract.graphics_apis == [ "opengl" ]
    ) "Flycast's package contract must remain an x86_64-linux OpenGL source build";
    assert lib.assertMsg (
      sourceContract.kind == "github"
      && builtins.match "^[0-9a-f]{40}$" sourceContract.revision != null
      && lib.hasPrefix "sha256-" sourceContract.sha256
    ) "Flycast's source contract must be an immutable GitHub revision";
    assert lib.assertMsg (
      sourceContract.tag == "v${packageContract.version}"
    ) "Flycast's source tag and package version must match";
    assert lib.assertMsg sourceContract.fetch_submodules
      "Flycast's exact source must include its pinned upstream submodules";
    assert lib.assertMsg (
      !fallbacks.host_executable
      && !fallbacks.flatpak
      && !fallbacks.libretro
      && !fallbacks.prebuilt_binary
    ) "Flycast's package contract must reject every runtime fallback";
    assert lib.assertMsg (
      renderHook.callbacks == [
        "get_proc_address"
        "current_context"
      ]
      && renderHook.framebuffer == "default_opengl_draw_framebuffer"
      && renderHook.presentation_aspect == "live final output windowbox width divided by height"
      && renderHook.layout_variant == "DEFAULT below 1.55 aspect and VARIANT_B at or above 1.55"
      && renderHook.origin == "bottom_left"
      && renderHook.orientation == "display_upright"
      && renderHook.rotation == 0
      && lib.hasInfix "semu_render_game_gl(&semu_render_frame);" semuPatchText
      && lib.hasInfix "semu_render_post_ui_gl(frame);" semuPatchText
      && lib.hasInfix "semu_render_frame.presentation_aspect" semuPatchText
      && lib.hasInfix "SEMU_RENDER_LAYOUT_VARIANT_B" semuPatchText
      && lib.hasInfix "semu_render_frame.surfaces[0].rotation = 0;" semuPatchText
      && !lib.hasInfix "surfaces[0].rotation = config::Rotate90" semuPatchText
    ) "Flycast's source patch must carry the direct two-phase renderer ABI";
    flycast.overrideAttrs (
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

        buildInputs = (previous.buildInputs or [ ]) ++ [
          systemdLibs
          semuRenderer
        ];

        postPatch = (previous.postPatch or "") + ''
          test -s ${semuRenderer}/include/${renderHook.header}
          game_source=${lib.escapeShellArg gamePhase.source}
          post_ui_source=${lib.escapeShellArg postUiPhase.source}
          frame_ready_line="$(grep -nF 'semu_render_frame_ready = BuildSemuRenderFrame();' "$game_source" | cut -d: -f1)"
          game_line="$(grep -nF 'semu_render_game_gl(&semu_render_frame);' "$game_source" | cut -d: -f1)"
          osd_line="$(grep -nF 'drawOSD();' "$game_source" | tail -n 1 | cut -d: -f1)"
          routing_line="$(grep -nF 'renderVideoRouting();' "$game_source" | tail -n 1 | cut -d: -f1)"
          post_line="$(grep -nF 'semu_render_post_ui_gl(frame);' "$post_ui_source" | cut -d: -f1)"
          present_line="$(grep -nF 'imguiDriver->present();' "$post_ui_source" | cut -d: -f1)"
          test "$frame_ready_line" -lt "$game_line"
          test "$game_line" -lt "$osd_line"
          test "$game_line" -lt "$routing_line"
          test "$post_line" -lt "$present_line"
          grep -Fq 'semu_render_frame.surfaces[0].native_width = framebuffer->getWidth();' "$game_source"
          grep -Fq 'semu_render_frame.surfaces[0].native_height = framebuffer->getHeight();' "$game_source"
          grep -Fq 'semu_render_frame.presentation_aspect' "$game_source"
          grep -Fq 'SEMU_RENDER_LAYOUT_VARIANT_B' "$game_source"
          grep -Fq 'semu_render_frame.surfaces[0].rotation = 0;' "$game_source"
          grep -Fq 'SEMU_RENDER_ORIGIN_BOTTOM_LEFT' "$game_source"
          ! grep -Fq 'surfaces[0].rotation = config::Rotate90' "$game_source"
        '';

        cmakeFlags = (previous.cmakeFlags or [ ]) ++ [
          (lib.cmakeBool "USE_HOST_SDL" true)
          (lib.cmakeBool "USE_OPENGL" true)
          (lib.cmakeBool "USE_VULKAN" false)
          (lib.cmakeBool "USE_BREAKPAD" false)
          (lib.cmakeBool "USE_DISCORD" false)
          (lib.cmakeBool "ENABLE_CTEST" false)
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
          license = lib.licenses.${metadata.license};
          mainProgram = outputContract.main_program_name;
          platforms = packageContract.platforms;
        };
      }
    );
in
assert lib.assertMsg (lib.elem stdenv.hostPlatform.system packageContract.platforms)
  "Semu Flycast supports only the package.json platform set";
sourceBuild
