# Cemu's package identity and source pin live in package.json. rendering.json
# owns the direct renderer boundaries interpreted by this source recipe.
{
  lib,
  stdenv,
  fetchFromGitHub,
  cemu,
  semuRenderer,
}:

let
  packageContract = builtins.fromJSON (builtins.readFile ./package.json);
  emulatorContract = builtins.fromJSON (builtins.readFile ./emulator.json);
  profileContract = builtins.fromJSON (builtins.readFile ./profile.json);
  rendering = (import ../../../packaging/nix/render-hook.nix { inherit lib; }) {
    emulatorDir = ./.;
  };
  sourceContract = packageContract.source;
  outputContract = packageContract.outputs;
  renderHook = rendering.hook;
  gamePhase = rendering.gamePhase;
  postUiPhase = rendering.postUiPhase;
  procedureResolver = renderHook.procedure_resolver;
  metadata = packageContract.metadata;
  linuxRuntime = emulatorContract.platforms.linux;
  launchContract = linuxRuntime.launch_contract;
  controllerContract = profileContract.controller;

  source = fetchFromGitHub {
    inherit (sourceContract) owner repo;
    rev = sourceContract.revision;
    hash = sourceContract.sha256;
  };

  semuPatch = rendering.patch;
  semuPatchHash = rendering.patchHash;
  addedPatchText = rendering.addedPatchText;

  buildContract = {
    schema_version = packageContract.schema_version;
    build_kind = packageContract.build_kind;
    version = packageContract.version;
    source_owner = sourceContract.owner;
    source_repo = sourceContract.repo;
    source_revision = sourceContract.revision;
    source_sha256 = sourceContract.sha256;
    patch_sha256 = semuPatchHash;
    executable = outputContract.main_program;
    linked_executable = outputContract.linked_program;
    launch_contract = launchContract;
    controller_profile = controllerContract;
    render_hook = renderHook;
    platforms = packageContract.platforms;
  };
  buildContractJson = builtins.toJSON buildContract;
in
assert lib.assertMsg (lib.elem stdenv.hostPlatform.system packageContract.platforms)
  "Semu Cemu has no package fallback outside its declared platform set";
assert lib.assertMsg (
  cemu.version == packageContract.version
) "The nixpkgs Cemu recipe must match package.json version ${packageContract.version}";
assert lib.assertMsg (
  !packageContract.fallbacks.host_executable
  && !packageContract.fallbacks.flatpak
  && !packageContract.fallbacks.libretro
  && !packageContract.fallbacks.prebuilt_binary
) "Cemu's package contract must forbid every non-source fallback";
assert lib.assertMsg (
  builtins.length linuxRuntime.args == 4
  && builtins.elemAt linuxRuntime.args 0 == launchContract.fullscreen_flag
  && builtins.elemAt linuxRuntime.args 1 == launchContract.mlc_flag
  && lib.hasInfix "state_root" (builtins.elemAt linuxRuntime.args 2)
  && builtins.elemAt linuxRuntime.args 3 == launchContract.game_flag
  && launchContract.command_line_failure_exit_code == 74
) "Cemu's declarative -f/-m/-g quick-launch contract is inconsistent";
assert lib.assertMsg (
  controllerContract.authoritative
  && controllerContract.compile_define == "SEMU_CEMU_OWNED_INPUT_PROFILE"
  && controllerContract.uuid == "${toString controllerContract.guid_index}_${controllerContract.guid}"
  && builtins.stringLength controllerContract.guid == 32
  && builtins.length controllerContract.mappings == 25
) "Cemu's generated player-one SDL profile identity is inconsistent";
assert lib.assertMsg (
  renderHook.surface_count == 1
  && lib.hasInfix "#include <semu_renderer.h>" addedPatchText
  && lib.hasInfix "#include \"${procedureResolver.loaded_table}\"" addedPatchText
  && lib.hasInfix "std::strcmp(name, STRINGIFY(function))" addedPatchText
  && lib.all (
    procedure: lib.hasInfix "GLFUNC(${procedure.type}, ${procedure.name})" addedPatchText
  ) procedureResolver.extensions
  && lib.hasInfix "semu_render_game_gl(&s_semu_frame);" addedPatchText
  && lib.hasInfix "semu_render_post_ui_gl(&s_semu_frame);" addedPatchText
  && lib.hasInfix "s_semu_frame.color_buffer = GL_BACK;" addedPatchText
  && lib.hasInfix "surfaces[0].rotation = 0;" addedPatchText
  && lib.hasInfix "#ifdef ${controllerContract.compile_define}" addedPatchText
  && lib.hasInfix "SEMU_CEMU_LAUNCH_FAILURE" addedPatchText
  && lib.hasInfix "exit(${toString launchContract.command_line_failure_exit_code});" addedPatchText
  && !lib.hasInfix "SEMU_GL_DIRECT" addedPatchText
  && !lib.hasInfix "_GetOpenGLFunction" addedPatchText
) "Cemu's patch must carry only the direct two-phase ABI 2 renderer hook";
cemu.overrideAttrs (
  previous:
  let
    previousEnvironment = previous.env or { };
  in
  {
    pname = packageContract.id;
    version = packageContract.version;
    src = source;
    patches = (previous.patches or [ ]) ++ [ semuPatch ];
    patchFlags = [
      "-p1"
      "--fuzz=0"
    ];
    buildInputs = lib.unique ((previous.buildInputs or [ ]) ++ [ semuRenderer ]);

    postPatch = (previous.postPatch or "") + ''
      test -s ${semuRenderer}/include/${renderHook.header}
      game_source=${lib.escapeShellArg gamePhase.source}
      post_source=${lib.escapeShellArg postUiPhase.source}
      header=src/Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h
      procedure_table=${lib.escapeShellArg "src/${procedureResolver.loaded_table}"}
      input_source=src/input/InputManager.cpp
      main_window=src/gui/MainWindow.cpp
      launch_source=src/config/LaunchSettings.cpp
      title_source=src/Cafe/TitleList/TitleInfo.cpp
      controller_factory=src/input/ControllerFactory.cpp
      sdl_controller=src/input/api/SDL/SDLController.cpp
      controller_header=src/input/api/Controller.h
      vpad_header=src/input/emulated/VPADController.h
      key_source=src/Cafe/Filesystem/FST/KeyCache.cpp
      active_settings=src/config/ActiveSettings.cpp
      main_source=src/main.cpp

      grep -Fq '("game,g", po::wvalue<std::wstring>()' "$launch_source"
      grep -Fq '("mlc,m", po::wvalue<std::wstring>()' "$launch_source"
      grep -Fq '("fullscreen,f", po::value<bool>()' "$launch_source"
      grep -Fq 'fs::exists(path / "content", ec) && fs::exists(path / "meta", ec) && fs::exists(path / "code", ec)' "$title_source"
      grep -Fq 'boost::iends_with(filenameStr, ".wua")' "$title_source"
      grep -Fq 'boost::iequals(filenameStr, "title.tmd")' "$title_source"
      grep -Fq 'InputManager::instance().load();' "$main_source"
      grep -Fq 'controllerProfiles/controller{}' "$input_source"
      grep -Fq 'const auto guid_index = ConvertString<size_t>(uuid.substr(0, index));' "$controller_factory"
      grep -Fq 'base_type(fmt::format("{}_", guid_index)' "$sdl_controller"
      grep -Fq 'enum Buttons2 : uint64' "$controller_header"
      grep -Fq 'enum ButtonId' "$vpad_header"
      grep -Fq '#ifdef ${controllerContract.compile_define}' "$input_source"
      grep -Fq 'SEMU_CEMU_LAUNCH_FAILURE' "$main_window"
      grep -Fq 'exit(${toString launchContract.command_line_failure_exit_code});' "$main_window"
      test "$(grep -Fc 'return SemuReportLaunchFailure' "$main_window")" -eq 7
      grep -Fq 'ActiveSettings::GetUserDataPath("keys.txt")' "$key_source"
      grep -Fq 'return GetUserDataPath("mlc01");' "$active_settings"
      grep -Fq 'usr/save/{:08X}/{:08X}/user/' src/Cafe/CafeSystem.cpp

      test "$(grep -Fc '#include <semu_renderer.h>' "$post_source")" -eq 1
      test "$(grep -Fc 'semu_render_game_gl(&s_semu_frame);' "$post_source")" -eq 1
      test "$(grep -Fc 'semu_render_post_ui_gl(&s_semu_frame);' "$post_source")" -eq 1
      grep -Fq 's_semu_frame.struct_size = sizeof(s_semu_frame);' "$post_source"
      grep -Fq 's_semu_frame.presentation_aspect' "$post_source"
      grep -Fq 'SEMU_RENDER_LAYOUT_VARIANT_B' "$post_source"
      grep -Fq 's_semu_frame.color_buffer = GL_BACK;' "$post_source"
      grep -Fq 's_semu_frame.surfaces[0].rotation = 0;' "$post_source"
      grep -Fq 's_semu_frame.get_proc = SemuGlProc;' "$post_source"
      grep -Fq 's_semu_frame.current_context = SemuCurrentContext;' "$post_source"
      grep -Fq '#include "${procedureResolver.loaded_table}"' "$post_source"
      grep -Fq 'std::strcmp(name, STRINGIFY(function))' "$post_source"
      ${lib.concatMapStringsSep "\n" (
        procedure: "grep -Fq 'GLFUNC(${procedure.type}, ${procedure.name})' \"$procedure_table\""
      ) procedureResolver.extensions}
      ! grep -Fq 'SEMU_GL_DIRECT' <<< ${lib.escapeShellArg addedPatchText}
      ! grep -Fq '_GetOpenGLFunction' <<< ${lib.escapeShellArg addedPatchText}
      test "$(grep -Fc 'void SemuRenderGame(' "$header")" -eq 1
      test "$(grep -Fc 'void SemuRenderPostUi();' "$header")" -eq 1

      draw_line="$(grep -nF 'g_renderer->DrawBackbufferQuad(' "$game_source" | cut -d: -f1)"
      game_line="$(grep -nF 'static_cast<OpenGLRenderer*>(g_renderer.get())->SemuRenderGame(' "$game_source" | cut -d: -f1)"
      screenshot_line="$(grep -nF 'g_renderer->HandleScreenshotRequest(' "$game_source" | cut -d: -f1)"
      imgui_line="$(grep -nF 'g_renderer->ImguiBegin(' "$game_source" | cut -d: -f1)"
      post_call_line="$(grep -nF 'SemuRenderPostUi();' "$post_source" | cut -d: -f1)"
      swap_line="$(grep -nF 'GLCanvas_SwapBuffers(swapTV, swapDRC);' "$post_source" | cut -d: -f1)"
      test "$draw_line" -lt "$game_line"
      test "$game_line" -lt "$screenshot_line"
      test "$screenshot_line" -lt "$imgui_line"
      test "$post_call_line" -lt "$swap_line"
    '';

    env = previousEnvironment // {
      NIX_CFLAGS_COMPILE =
        (previousEnvironment.NIX_CFLAGS_COMPILE or "")
        + " -D${renderHook.compile_define}"
        + " -D${controllerContract.compile_define}"
        + " -I${semuRenderer}/include";
      NIX_LDFLAGS =
        (previousEnvironment.NIX_LDFLAGS or "")
        + " -L${semuRenderer}/lib -rpath ${semuRenderer}/lib"
        + " -l${renderHook.library}";
    };

    cmakeFlags = (previous.cmakeFlags or [ ]) ++ [
      (lib.cmakeBool "ENABLE_OPENGL" true)
      (lib.cmakeBool "ENABLE_VULKAN" false)
    ];

    postInstall = (previous.postInstall or "") + ''
      mkdir -p "$out/share/semu"
      printf '%s\n' ${lib.escapeShellArg buildContractJson} \
        > "$out/${outputContract.build_contract}"
    '';

    doInstallCheck = true;
    installCheckPhase = ''
      runHook preInstallCheck
      launcher="$out/${outputContract.main_program}"
      binary="$out/${outputContract.linked_program}"
      contract="$out/${outputContract.build_contract}"
      test -x "$launcher"
      test -x "$binary"
      test -s "$contract"
      test "$(cat "$contract")" = ${lib.escapeShellArg buildContractJson}
      grep -aFq '${gamePhase.symbol}' "$binary"
      grep -aFq '${postUiPhase.symbol}' "$binary"
      readelf -d "$binary" > dynamic-section.txt
      grep -Fq 'lib${renderHook.library}.so' dynamic-section.txt
      grep -Fq '${semuRenderer}/lib' dynamic-section.txt
      runHook postInstallCheck
    '';

    passthru = (previous.passthru or { }) // {
      semuBuildContract = buildContract;
      semuRenderHook = renderHook;
    };

    meta = (previous.meta or { }) // {
      inherit (metadata) description homepage;
      license = lib.licenses.${metadata.license};
      mainProgram = builtins.baseNameOf outputContract.main_program;
      platforms = packageContract.platforms;
    };
  }
)
