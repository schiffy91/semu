# RetroArch source recipes. Linux glcore directly links the shared renderer ABI;
# macOS remains a separate Metal build of the same pinned source.
{
  lib,
  stdenv,
  fetchFromGitHub,
  fetchurl,
  unzip,
  pkg-config,
  zlib,
  ffmpeg_7,
  qt6,
  retroarch-bare,
  btrcpy,
  semuRenderer,
  wrapGAppsHook3,
  libretro,
  runCommand,
  symlinkJoin,
  writeTextDir,
  retroarch-joypad-autoconfig,
  coreHostId ? "",
  selectedCoreIds ? [ ],
}:

let
  packageContract = builtins.fromJSON (builtins.readFile ./package.json);
  profileContract = builtins.fromJSON (builtins.readFile ./profile.json);
  coreManifest = builtins.fromJSON (builtins.readFile ./cores.json);
  emulatorContract = builtins.fromJSON (builtins.readFile ./emulator.json);
  rendering = (import ../../../packaging/nix/render-hook.nix { inherit lib; }) {
    emulatorDir = ./.;
  };
  sourceContract = packageContract.source;
  outputContract = packageContract.outputs;
  renderHook = rendering.hook;
  renderHookEnabled = lib.elem stdenv.hostPlatform.system renderHook.platforms;
  surfaceModes = rendering.contract.surface_modes;
  singleMode = lib.findFirst (
    mode: mode.match == { screen_count = 1; }
  ) null surfaceModes;
  verticalMode = lib.findFirst (
    mode:
    mode.match
      == {
        screen_count = 2;
        arrangement = "vertical";
      }
  ) null surfaceModes;
  pointerMapping = rendering.contract.integration.pointer_mapping;
  profileLines = (builtins.head profileContract.files).lines;
  version = packageContract.version;

  # ONE pinned source for every platform variant: tag v1.22.2, resolved
  # 2026-07-02 via git ls-remote (pin the commit, not the movable tag).
  source = fetchFromGitHub {
    inherit (sourceContract) owner repo;
    rev = sourceContract.revision;
    hash = sourceContract.sha256;
  };

  coreImplementation =
    core: platform:
    let
      definition =
        coreManifest.implementations.${core}
          or (throw "retroarch package: selected core '${core}' has no local implementation");
    in
    definition.${platform}
      or (throw "retroarch package: selected core '${core}' has no ${platform} implementation");

  linuxCoreResolution =
    core:
    let
      implementation = coreImplementation core "linux";
      packageAttribute = implementation.package_attribute;
      package =
        libretro.${packageAttribute} or (
          throw "retroarch package: nixpkgs libretro has no '${packageAttribute}' package"
          + " for selected core '${core}'"
        );
    in
    assert lib.assertMsg (
      implementation.kind == "nixpkgs_libretro"
    ) "retroarch package: Linux core '${core}' is not source-built by Nix";
    assert lib.assertMsg (lib.isDerivation package)
      "retroarch package: selected core '${core}' did not resolve to a derivation";
    {
      name = core;
      inherit package;
      buildKind = implementation.kind;
    };

  linuxCoreResolutions = map linuxCoreResolution selectedCoreIds;
  coreBuildContract =
    resolution:
    let
      package = resolution.package;
      packageSource = package.src or null;
      sourceIsAttrs = builtins.isAttrs packageSource;
    in
    {
      name = resolution.name;
      library = "lib/retroarch/cores/${resolution.name}_libretro.so";
      build_kind = resolution.buildKind;
      package_name = package.pname or (lib.getName package);
      version = if package ? version then toString package.version else null;
      nix_store_path = toString package;
      source_revision = if sourceIsAttrs then packageSource.rev or null else null;
      source_sha256 = if sourceIsAttrs then packageSource.outputHash or null else null;
      source_url = if sourceIsAttrs then packageSource.url or null else null;
    };
  linuxCoreBuilds = map coreBuildContract linuxCoreResolutions;
  coreBuildContractPath = packageContract.core_runtime.build_contract;
  coreBuildContractDirectory = builtins.dirOf coreBuildContractPath;
  completeCoreBuild =
    build:
    builtins.isString build.package_name
    && build.package_name != ""
    && builtins.isString build.version
    && build.version != ""
    && lib.hasPrefix "/nix/store/" build.nix_store_path
    && builtins.isString build.source_sha256
    && lib.hasPrefix "sha256-" build.source_sha256
    && builtins.isString build.source_url
    && lib.hasPrefix "https://" build.source_url
    && builtins.isString build.source_revision
    && builtins.stringLength build.source_revision == 40
    && builtins.match "^[0-9a-f]+$" build.source_revision != null;
  incompleteCoreBuilds = map (build: build.name) (
    lib.filter (build: !(completeCoreBuild build)) linuxCoreBuilds
  );
  linuxCoreContract =
    assert lib.assertMsg (
      incompleteCoreBuilds == [ ]
    ) "retroarch package: selected core metadata is incomplete: ${toString incompleteCoreBuilds}";
    {
      schema_version = 1;
      host = coreHostId;
      platform = stdenv.hostPlatform.system;
      selected_core_ids = selectedCoreIds;
      cores = linuxCoreBuilds;
    };
  linuxCoreBundle =
    assert lib.assertMsg (
      selectedCoreIds != [ ]
    ) "retroarch package: a Linux core host requires a non-empty selection";
    builtins.deepSeq linuxCoreContract (symlinkJoin {
      name = "semu-${coreHostId}-cores";
      paths = map (resolution: resolution.package) linuxCoreResolutions;
      postBuild = ''
        mkdir -p "$out/${coreBuildContractDirectory}"
        cat > "$out/share/semu/core-host-selection.txt" <<'CORES'
        ${lib.concatStringsSep "\n" selectedCoreIds}
        CORES
        printf '%s\n' ${lib.escapeShellArg (builtins.toJSON linuxCoreContract)} \
          > "$out/${coreBuildContractPath}"

        while IFS= read -r core; do
          test -s "$out/lib/retroarch/cores/''${core}_libretro.so" || {
            echo "contract core has no staged library: $core" >&2
            exit 1
          }
        done < "$out/share/semu/core-host-selection.txt"

        expected=${toString (lib.length selectedCoreIds)}
        actual="$(find "$out/lib/retroarch/cores" -mindepth 1 -maxdepth 1 \
          -name '*_libretro.so' -print | wc -l)"
        test "$actual" -eq "$expected" || {
          echo "staged core count $actual does not match selected contract $expected" >&2
          exit 1
        }
      '';
      passthru = {
        coreNames = selectedCoreIds;
        coreCount = lib.length selectedCoreIds;
        coreContract = linuxCoreContract;
        coreContractPath = coreBuildContractPath;
      };
    });

  runtimeExtraContracts = packageContract.runtime_extras;
  steamDeckAutoconfigContract = runtimeExtraContracts.input_autoconfig;
  steamDeckAutoconfigManifest = lib.importJSON (./. + "/${steamDeckAutoconfigContract.manifest}");
  safeAutoconfig = lib.all (
    profile:
    builtins.baseNameOf profile.file == profile.file
    && lib.hasSuffix ".cfg" profile.file
    && profile.lines != [ ]
  ) steamDeckAutoconfigManifest.profiles;
  steamDeckAutoconfig =
    assert lib.assertMsg (
      steamDeckAutoconfigContract.kind == "merged_input_autoconfig"
      && !lib.hasPrefix "/" steamDeckAutoconfigContract.destination
      && !lib.hasInfix ".." steamDeckAutoconfigContract.destination
      && steamDeckAutoconfigManifest.schema_version == 1
      && safeAutoconfig
    ) "retroarch package: invalid Steam Deck input autoconfig manifest";
    symlinkJoin {
      name = "semu-${coreHostId}-owned-autoconfig";
      paths = map (
        profile:
        writeTextDir "${steamDeckAutoconfigContract.destination}/${profile.file}" (
          lib.concatStringsSep "\n" profile.lines + "\n"
        )
      ) steamDeckAutoconfigManifest.profiles;
    };
  inputAutoconfig = runCommand "semu-${coreHostId}-autoconfig" { } ''
    mkdir -p "$out/share/libretro"
    cp -R ${retroarch-joypad-autoconfig}/share/libretro/autoconfig \
      "$out/share/libretro/autoconfig"
    chmod -R u+w "$out/share/libretro/autoconfig"
    cp -RL ${steamDeckAutoconfig}/share/libretro/autoconfig/. \
      "$out/share/libretro/autoconfig/"
  '';
  declaredRuntimeExtraIds = lib.attrNames (
    lib.filterAttrs (
      _: extra: lib.elem stdenv.hostPlatform.system extra.platforms
    ) runtimeExtraContracts
  );
  runtimeExtras = lib.optionalAttrs stdenv.hostPlatform.isLinux {
    input_autoconfig = inputAutoconfig;
    selected_cores = linuxCoreBundle;
  };
  validRuntimeExtras =
    lib.sort builtins.lessThan (lib.attrNames runtimeExtras)
    == lib.sort builtins.lessThan declaredRuntimeExtraIds
    && lib.all lib.isDerivation (lib.attrValues runtimeExtras);

  auxiliaryPatchContracts = packageContract.auxiliary_patches;
  networkCommandPatchContract = auxiliaryPatchContracts.network_commands;
  networkCommandPatch = ./retroarch_commands.patch;
  networkCommandPatchHash = builtins.convertHash {
    hash = builtins.hashFile "sha256" networkCommandPatch;
    hashAlgo = "sha256";
    toHashFormat = "sri";
  };

  statusNullSafetyPatchContract = auxiliaryPatchContracts.status_null_safety;
  statusNullSafetyPatch = ./retroarch_get_status_null_safety.patch;
  statusNullSafetyPatchHash = builtins.convertHash {
    hash = builtins.hashFile "sha256" statusNullSafetyPatch;
    hashAlgo = "sha256";
    toHashFormat = "sri";
  };

  semuPatch = rendering.patch;
  semuPatchHash = rendering.patchHash;
  semuHookDirectory = ../../../src/generators/rendering/retroarch;
  semuHookSource = lib.fileset.toSource {
    root = semuHookDirectory;
    fileset = semuHookDirectory;
  };
  semuHookText = builtins.readFile (semuHookDirectory + "/runtime_bridge.btrc")
    + builtins.readFile (semuHookDirectory + "/surface_contract.btrc");
  semuHookHash = builtins.convertHash {
    hash = builtins.hashString "sha256" semuHookText;
    hashAlgo = "sha256";
    toHashFormat = "sri";
  };
  semuDarwinPatch = ./retroarch_darwin.patch;
  semuDarwinPatchHash = builtins.hashFile "sha256" ./retroarch_darwin.patch;

  sharedPatches = [
    networkCommandPatch
    statusNullSafetyPatch
  ];
  semuPatchText = rendering.patchText;
  semuPatchLines = lib.splitString "\n" semuPatchText;
  semuHookLines = lib.splitString "\n" semuHookText;
  containsLine = fragment: lines: lib.any (line: lib.hasInfix fragment line) lines;
  linuxCoreDirectories = emulatorContract.platforms.linux.core_loader.directories;
  expectedLinuxCoreDirectories = [
    "\${asset_root}/lib/retroarch/cores"
  ];
  buildContract = {
    schema_version = packageContract.schema_version;
    id = packageContract.id;
    inherit version;
    build_kind = packageContract.build_kind;
    platform = stdenv.hostPlatform.system;
    source_owner = sourceContract.owner;
    source_repo = sourceContract.repo;
    source_revision = sourceContract.revision;
    source_sha256 = sourceContract.sha256;
    patch_sha256 = if renderHookEnabled then semuPatchHash else semuDarwinPatchHash;
    hook_source_sha256 = if renderHookEnabled then semuHookHash else null;
    auxiliary_patch_sha256s = {
      network_commands = networkCommandPatchHash;
      status_null_safety = statusNullSafetyPatchHash;
    };
    executable =
      if stdenv.hostPlatform.isDarwin then
        outputContract.macos_bundle_program
      else
        outputContract.linux_real_program;
    core_names = selectedCoreIds;
    render_hook = if renderHookEnabled then renderHook else null;
  };
  buildContractJson = builtins.toJSON buildContract;

  # --- Linux directly linked OpenGL build ---------------------------------
  linuxRendererBuild =
    assert lib.assertMsg (
      packageContract.build_kind == "source"
      && sourceContract.kind == "github"
      && packageContract.fallbacks.linux_host_executable == false
      && packageContract.fallbacks.linux_flatpak == false
      && packageContract.fallbacks.linux_prebuilt_binary == false
      && packageContract.fallbacks.macos_host_executable == false
      && packageContract.fallbacks.macos_prebuilt_binary == false
      && packageContract.core_runtime.exact_selection
      && packageContract.core_runtime.environment_search == false
      && packageContract.core_runtime.host_search == false
      && packageContract.core_runtime.directory == "lib/retroarch/cores"
      && !lib.hasPrefix "/" coreBuildContractPath
      && !lib.hasInfix ".." coreBuildContractPath
      && lib.hasSuffix "-build-contract.json" coreBuildContractPath
      && packageContract.core_host.enabled
      && packageContract.core_host.selector_field == "core"
      && packageContract.core_host.implementation_manifest == "cores.json"
      && networkCommandPatchContract.kind == "local_exact"
      && networkCommandPatchContract.file == "retroarch_commands.patch"
      && networkCommandPatchContract.sha256 == networkCommandPatchHash
      && statusNullSafetyPatchContract.kind == "local_exact"
      && statusNullSafetyPatchContract.file
        == "retroarch_get_status_null_safety.patch"
      && statusNullSafetyPatchContract.sha256 == statusNullSafetyPatchHash
      && coreManifest.schema_version == 1
      && validRuntimeExtras
      && linuxCoreDirectories == expectedLinuxCoreDirectories
    ) "RetroArch and its cores must resolve only from the source-built AppImage slice";
    assert lib.assertMsg (
      renderHook.abi == 3
      && renderHook.callbacks == [
        "get_proc_address"
        "context_generation"
      ]
      && renderHook.context == {
        driver = "glcore";
        minimum_glsl = 330;
        minimum_opengl = "3.3";
      }
      && renderHook.frame_gate
        == "video_info.libretro_running with valid game and viewport geometry; native menu state gates pointer publication but not game treatment"
      && renderHook.video_threading
        == "disabled_to_keep_gl_context_creation_rendering_recreation_and_destroy_on_one_owner_thread"
      && builtins.elem "video_threaded = \"false\"" profileLines
      && containsLine "semu_render_game_gl(&semu_frame);" semuPatchLines
      && containsLine "semu_render_post_ui_gl(&semu_frame);" semuPatchLines
      && containsLine "video_driver_semu_build_frame_gl(&semu_frame" semuPatchLines
      && containsLine "video_driver_semu_context_created()" semuPatchLines
      && containsLine "video_driver_semu_context_destroying(" semuPatchLines
      && containsLine "video_driver_semu_pointer_sample(" semuPatchLines
      && containsLine "video_driver_semu_pointer_clear(true);" semuPatchLines
      && containsLine "RARCH_DEVICE_MOUSE_SCREEN" semuPatchLines
      && containsLine "RETRO_DEVICE_ID_MOUSE_X" semuPatchLines
      && containsLine "RETRO_DEVICE_ID_MOUSE_Y" semuPatchLines
      && containsLine "RETRO_DEVICE_ID_MOUSE_LEFT" semuPatchLines
      && !containsLine "RARCH_DEVICE_POINTER_SCREEN" semuPatchLines
      && containsLine "SEMU_RENDER_SURFACE_COUNT" semuHookLines
      && containsLine "frame->context_generation" semuHookLines
      && containsLine "semu_render_context_invalidate_gl(" semuHookLines
      && containsLine "SemuRetroArchPointerBridge.sample(" semuHookLines
      && !containsLine "DeSmuME" semuPatchLines
      && !containsLine "DeSmuME" semuHookLines
    ) "RetroArch must expose minimal ABI-3 tap points backed by the strict BTRC hook";
    assert lib.assertMsg (
      builtins.length surfaceModes == 2
      && singleMode != null
      && singleMode.id == "single"
      && singleMode.validation == {
        live_geometry = "positive";
        native_widths = "independent";
      }
      && builtins.length singleMode.surfaces == 1
      && (builtins.elemAt singleMode.surfaces 0).index == 0
      && verticalMode != null
      && verticalMode.id == "dual_vertical"
      && verticalMode.validation == {
        live_geometry = "uniform_integer_scale";
        native_widths = "equal";
      }
      && builtins.length verticalMode.surfaces == 2
      && (builtins.elemAt verticalMode.surfaces 0).index == 0
      && (builtins.elemAt verticalMode.surfaces 1).index == 1
      && (builtins.elemAt verticalMode.surfaces 0).id
        != (builtins.elemAt verticalMode.surfaces 1).id
      && pointerMapping.surface_id == "SEMU_RENDER_TOUCH_SURFACE_ID"
      && pointerMapping.surface_index == "SEMU_RENDER_TOUCH_SURFACE_INDEX"
      && pointerMapping.viewport == "retroarch_absolute_output_mouse"
      && pointerMapping.coordinates
        == "absolute_window_pixels_to_libretro_gapless_top_bottom_normalized"
      && pointerMapping.press_clamp == false
      && pointerMapping.captured_motion_clamp == true
    ) "RetroArch must declare compiler-selected single and vertical surface modes";
    (
      (retroarch-bare.override {
        withGamemode = false;
        withVulkan = false;
        withWayland = false;
      }).overrideAttrs
      (previous: {
        pname = "retroarch-semu";
        inherit version;
        src = source;

        buildInputs =
          (lib.subtractLists [
            ffmpeg_7
            qt6.qtbase
          ] (previous.buildInputs or [ ]))
          ++ lib.optionals renderHookEnabled [ semuRenderer ];
        nativeBuildInputs = lib.subtractLists [
          qt6.wrapQtAppsHook
          wrapGAppsHook3
        ] (previous.nativeBuildInputs or [ ])
          ++ lib.optionals renderHookEnabled [ btrcpy ];

        dontWrapQtApps = true;
        dontWrapGApps = true;

        configureFlags = (previous.configureFlags or [ ]) ++ [
          "--disable-qt"
          "--disable-ffmpeg"
          "--disable-vulkan"
          "--disable-wayland"
          "--enable-threads"
        ];

        patches =
          (previous.patches or [ ]) ++ sharedPatches ++ lib.optionals renderHookEnabled [ semuPatch ];
        patchFlags = (previous.patchFlags or [ "-p1" ]) ++ [ "--fuzz=0" ];

        postPatch =
          (previous.postPatch or "")
          + ''
            test "$(grep -hF 'bool command_save_state_slot(command_t* cmd, const char* arg)' \
              command.c command.h | wc -l)" -eq 2
            test "$(grep -Fc '{ "SAVE_STATE_SLOT",command_save_state_slot, "<slot number>"}' \
              command.h)" -eq 1
            test "$(grep -Fc 'string_is_equal(arg, "menu_active")' \
              command.c)" -eq 1
          ''
          + lib.optionalString renderHookEnabled ''
            test -s ${semuRenderer}/include/${renderHook.header}
            btrcpy ${semuHookSource}/runtime_bridge.btrc \
              -o gfx/semu_retroarch.c \
              --strict-imports --no-cache --no-stdlib --no-dce
            test -s gfx/semu_retroarch.c
            hook_source=gfx/semu_retroarch.c
            input_source=input/input_driver.c
            test "$(grep -hF 'semu_render_game_gl(&semu_frame);' \
              gfx/drivers/gl3.c | wc -l)" -eq 1
            test "$(grep -hF 'semu_render_post_ui_gl(&semu_frame);' \
              gfx/drivers/gl3.c | wc -l)" -eq 1
            test "$(grep -hF 'video_driver_semu_build_frame_gl(&semu_frame,' \
              gfx/drivers/gl3.c | wc -l)" -eq 1
            test "$(grep -hF 'video_driver_semu_publish_pointer(' \
              gfx/drivers/gl3.c | wc -l)" -eq 1
            ! grep -hFq 'semu_frame.surface' gfx/drivers/gl3.c

            test "$(grep -Fc 'bool video_driver_semu_build_frame_gl(' \
              "$hook_source")" -eq 2
            grep -Fq 'getenv("SEMU_RENDER_SURFACE_COUNT")' "$hook_source"
            grep -Fq 'getenv("SEMU_RENDER_SURFACE_LAYOUT")' "$hook_source"
            grep -Fq 'getenv("SEMU_RENDER_SURFACE_NATIVE_WIDTH_POLICY")' \
              "$hook_source"
            grep -Fq 'getenv("SEMU_RENDER_SURFACE_GEOMETRY_POLICY")' \
              "$hook_source"
            grep -Eq 'surface_count[[:space:]]*=[[:space:]]*2' "$hook_source"
            grep -Eq 'context_generation[[:space:]]*=[[:space:]]*contextGeneration' \
              "$hook_source"
            grep -Fq 'semu_render_context_invalidate_gl(contextGeneration)' \
              "$hook_source"
            grep -Fq 'surface_index = touchIndex' "$hook_source"
            grep -Fq 'wasCaptured != 0) ? 1 : 0' "$hook_source"
            grep -Fq 'SemuRetroArchPointer.map_generation == mapGeneration' \
              "$hook_source"
            grep -Fq 'slock_lock(video->display_lock)' "$hook_source"
            grep -Fq 'slock_lock(video->context_lock)' "$hook_source"

            grep -Fq 'int semu_game_status = SEMU_RENDER_PASSTHROUGH;' \
              gfx/drivers/gl3.c
            grep -Fq '(void)semu_render_post_ui_gl(&semu_frame);' \
              gfx/drivers/gl3.c
            grep -Fq 'semu_game_status == SEMU_RENDER_APPLIED' \
              gfx/drivers/gl3.c
            ! grep -Fq 'semu_post_ui_status' gfx/drivers/gl3.c

            destroy_line="$(grep -nF 'video_driver_semu_context_destroying(' \
              gfx/drivers/gl3.c | cut -d: -f1 | tail -1)"
            resources_line="$(grep -nF 'gl3_destroy_resources(gl);' \
              gfx/drivers/gl3.c | cut -d: -f1 | tail -1)"
            context_line="$(grep -nF 'gl->ctx_driver->destroy(gl->ctx_data);' \
              gfx/drivers/gl3.c | cut -d: -f1 | tail -1)"
            test "$destroy_line" -lt "$resources_line"
            test "$resources_line" -lt "$context_line"

            test "$(grep -Fc 'static void input_driver_semu_poll_pointer(' \
              "$input_source")" -eq 1
            grep -Fq 'RARCH_DEVICE_MOUSE_SCREEN' "$input_source"
            grep -Fq 'RETRO_DEVICE_ID_MOUSE_X' "$input_source"
            grep -Fq 'RETRO_DEVICE_ID_MOUSE_Y' "$input_source"
            grep -Fq 'RETRO_DEVICE_ID_MOUSE_LEFT' "$input_source"
            ! grep -Fq 'RARCH_DEVICE_POINTER_SCREEN' "$input_source"
            grep -Fq 'input_driver_semu_poll_pointer(' "$input_source"
            grep -Fq 'video_driver_semu_pointer_snapshot(' "$input_source"
            ! grep -Fq 'DeSmuME' "$hook_source" "$input_source"
            for driver in gfx/drivers/gl3.c; do
              game_line="$(grep -nF 'semu_render_game_gl(&semu_frame);' "$driver" | cut -d: -f1)"
              completed_draw='gl3_filter_chain_end_frame'
              completed_line="$(grep -nF "$completed_draw" "$driver" | \
                awk -F: -v game="$game_line" '$1 < game { line = $1 } END { print line }')"
              menu_line="$(grep -nF 'menu_driver_frame(menu_is_alive, video_info);' "$driver" | cut -d: -f1)"
              widget_line="$(grep -nF 'gfx_widgets_frame(video_info);' "$driver" | cut -d: -f1)"
              post_line="$(grep -nF 'semu_render_post_ui_gl(&semu_frame);' "$driver" | cut -d: -f1)"
              publish_line="$(grep -nF 'video_driver_semu_publish_pointer(' "$driver" | cut -d: -f1)"
              swap_line="$(grep -nF 'ctx_driver->swap_buffers' "$driver" | \
                awk -F: -v publish="$publish_line" '$1 > publish { print $1; exit }')"
              test -n "$completed_line"
              test "$completed_line" -lt "$game_line"
              test "$game_line" -lt "$menu_line"
              test "$menu_line" -lt "$widget_line"
              test "$widget_line" -lt "$post_line"
              test "$post_line" -lt "$publish_line"
              test "$publish_line" -lt "$swap_line"
            done
          '';

        env =
          (previous.env or { })
          // lib.optionalAttrs renderHookEnabled {
            NIX_CFLAGS_COMPILE =
              (previous.env.NIX_CFLAGS_COMPILE or "")
              + " -D${renderHook.compile_define} -I${semuRenderer}/include";
            NIX_LDFLAGS =
              (previous.env.NIX_LDFLAGS or "")
              + " -L${semuRenderer}/lib -rpath ${semuRenderer}/lib -l${renderHook.library}";
          };

        # Keep one real ELF and one relative alias. No program wrapper participates
        # in either public executable's resolution.
        postInstall = (previous.postInstall or "") + ''
          ln -s retroarch "$out/bin/retroarch-semu"
          mkdir -p "$out/share/semu"
          printf '%s\n' ${lib.escapeShellArg buildContractJson} \
            > "$out/${outputContract.build_contract}"
        '';

        postFixup = (previous.postFixup or "") + ''
          target="$out/bin/retroarch"
          alias="$out/bin/retroarch-semu"

          test -f "$target"
          test ! -L "$target"
          readelf -h "$target" >/dev/null
          ${lib.optionalString renderHookEnabled ''
            grep -aFq semu_render_game_gl "$target"
            grep -aFq semu_render_post_ui_gl "$target"
            dynamic_section="$(readelf -d "$target")"
            grep -Fq 'lib${renderHook.library}.so' <<<"$dynamic_section"
            grep -Fq '${semuRenderer}/lib' <<<"$dynamic_section"
          ''}

          test -L "$alias"
          test "$(readlink "$alias")" = retroarch
          test "$(readlink -f "$target")" = "$(readlink -f "$alias")"
          test "$(find "$out/bin" -mindepth 1 -maxdepth 1 | wc -l)" -eq 2
          contract="$out/${outputContract.build_contract}"
          test -s "$contract"
          test "$(cat "$contract")" = ${lib.escapeShellArg buildContractJson}

          test -z "$(find "$out" -name '.*retroarch*wrapped*' -print -quit)"
          if grep -R -aFq \
            -e QT_PLUGIN_PATH \
            -e GDK_PIXBUF_MODULE_FILE \
            "$out/bin"; then
            echo "RetroArch wrapper metadata survived fixup" >&2
            exit 1
          fi
        '';

        passthru = (previous.passthru or { }) // {
          semuBuildContract = buildContract;
          semuRenderHook = renderHook;
          semuCoreHostPackage = linuxCoreBundle;
          semuRuntimeExtras = runtimeExtras;
        };
      })
    );

  # --- macOS build: pinned source with hash-pinned local core declarations --
  macArch = if stdenv.hostPlatform.isAarch64 then "arm64" else "x86_64";
  macCoreZip =
    core:
    let
      implementation = coreImplementation core "macos";
      artifact = implementation.artifact;
      hash =
        implementation.sha256.${stdenv.hostPlatform.system}
          or (throw "retroarch package: no macOS hash for '${core}' on ${stdenv.hostPlatform.system}");
    in
    assert lib.assertMsg (
      implementation.kind == "libretro_buildbot_archive"
    ) "retroarch package: invalid macOS core implementation for '${core}'";
    fetchurl {
      url = "https://buildbot.libretro.com/nightly/apple/osx/${macArch}/latest/${artifact}_libretro.dylib.zip";
      name = "${artifact}_libretro.dylib.zip";
      inherit hash;
    };

  macBuild = stdenv.mkDerivation {
    pname = "retroarch";
    inherit version;
    src = source;
    patches = sharedPatches ++ [ semuDarwinPatch ];
    patchFlags = [
      "-p1"
      "--fuzz=0"
    ];

    nativeBuildInputs = [
      pkg-config
      unzip
    ];
    buildInputs = [ zlib ];

    # qb configure (not autoconf): Metal + the vendored (builtin) glslang /
    # SPIRV-Cross slang pipeline the Metal driver requires. nixpkgs zlib
    # replaces the vendored deps/libz (whose classic-MacOS fdopen macro
    # breaks against modern SDK headers). The self-update paths are disabled
    # so RetroArch never writes into the store; Qt is the desktop companion
    # UI Semu never launches; ffmpeg recording is off to keep the closure at
    # the SDK frameworks.
    configureFlags = [
      "--enable-metal"
      "--enable-builtinglslang"
      "--disable-builtinzlib"
      "--disable-qt"
      "--disable-ffmpeg"
      # the make build never compiles the CoreAudio microphone driver the
      # Xcode project carries; Semu has no microphone use anyway
      "--disable-microphone"
      "--disable-update_cores"
      "--disable-update_assets"
      "--disable-update_core_info"
    ];

    enableParallelBuilding = true;

    # `make install` puts a naked Mach-O at bin/retroarch, but the Cocoa entry
    # point calls NSApplicationMain. Without the Xcode target's bundle metadata
    # and nib, NSApplicationMain has no RetroArch_OSX delegate: the process
    # enters the AppKit event loop before rarch_main and hangs (even --help).
    # Materialize the real application bundle the binary's ABI requires, then
    # keep the CLI contract as a small wrapper which execs that exact Mach-O.
    postInstall = ''
      app="$out/Applications/RetroArch.app"
      mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources" $out/libexec $out/cores
      mv $out/bin/retroarch "$app/Contents/MacOS/RetroArch"

      # The pinned source owns the matching Cocoa resources. Copy the complete
      # resource set so Metal's default library and the nib-created window /
      # RetroArch_OSX delegate cannot drift independently from the executable.
      cp -R ${source}/pkg/apple/OSX/Resources/. "$app/Contents/Resources/"
      cp ${source}/pkg/apple/OSX/Info_Metal.plist "$app/Contents/Info.plist"
      # The make build's LC_BUILD_VERSION is minos 11.0; keep LaunchServices
      # metadata equally permissive instead of advertising the SDK version.
      substituteInPlace "$app/Contents/Info.plist" \
        --replace-fail '$(EXECUTABLE_NAME)' 'RetroArch' \
        --replace-fail '$(PRODUCT_BUNDLE_IDENTIFIER)' 'com.libretro.RetroArch' \
        --replace-fail \''${PRODUCT_NAME} 'RetroArch' \
        --replace-fail '$(MARKETING_VERSION)' '${version}' \
        --replace-fail '$(CURRENT_PROJECT_VERSION)' '${version}' \
        --replace-fail '$(MACOSX_DEPLOYMENT_TARGET)' '11.0'

      # Compatibility path for diagnostics which used $out/libexec directly.
      # Production launches use the bundle path below so NSBundle discovery is
      # explicit rather than dependent on symlink resolution.
      ln -s ../Applications/RetroArch.app/Contents/MacOS/RetroArch \
        $out/libexec/retroarch

      test -x "$app/Contents/MacOS/RetroArch"
      test -s "$app/Contents/Resources/en.lproj/MainMenu_Metal.nib"
      test -s "$app/Contents/Resources/default.metallib"
      grep -q '<string>RetroArch</string>' "$app/Contents/Info.plist"
      grep -q '<string>MainMenu_Metal</string>' "$app/Contents/Info.plist"
      if grep -qE '\$\(|\$\{' "$app/Contents/Info.plist"; then
        echo "unexpanded Xcode variable in RetroArch Info.plist" >&2
        exit 1
      fi

      ${lib.concatMapStringsSep "\n" (core: "unzip -o ${macCoreZip core} -d $out/cores/") selectedCoreIds}

      echo "libretro_directory = \"$out/cores\"" > $out/cores/path.cfg
      cat > $out/bin/retroarch <<'WRAPPER'
      #!/bin/bash
      SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
      CONFIG="''${SEMU_RETROARCH_CONFIG:-$HOME/Library/Application Support/RetroArch/retroarch.cfg}"
      APPEND_CONFIG="$SCRIPT_DIR/cores/path.cfg"
      if [ -n "''${SEMU_RETROARCH_APPEND_CONFIG:-}" ]; then
        APPEND_CONFIG="$APPEND_CONFIG|$SEMU_RETROARCH_APPEND_CONFIG"
      fi
      exec "$SCRIPT_DIR/Applications/RetroArch.app/Contents/MacOS/RetroArch" \
        --config="$CONFIG" \
        --appendconfig="$APPEND_CONFIG" \
        "$@"
      WRAPPER
      chmod +x $out/bin/retroarch

      mkdir -p "$out/share/semu"
      printf '%s\n' ${lib.escapeShellArg buildContractJson} \
        > "$out/${outputContract.build_contract}"
      test "$(cat "$out/${outputContract.build_contract}")" = \
        ${lib.escapeShellArg buildContractJson}
    '';

    passthru = {
      semuBuildContract = buildContract;
      semuRenderHook = renderHook // {
        enabled = false;
      };
      semuCoreHostPackage = null;
      semuRuntimeExtras = runtimeExtras;
    };

    meta = {
      description = "RetroArch (Metal, built from source) with the ${toString (lib.length selectedCoreIds)} selected cores";
      homepage = "https://www.retroarch.com";
      platforms = lib.platforms.darwin;
      license = lib.licenses.gpl3;
      mainProgram = "retroarch";
    };
  };
in
assert lib.assertMsg (
  packageContract.core_host.enabled
  && coreHostId != ""
  && packageContract.core_host.selector_field == "core"
  && packageContract.core_host.implementation_manifest == "cores.json"
  && coreManifest.schema_version == 1
  && validRuntimeExtras
) "retroarch package: invalid core-host or runtime-extra declaration";
if stdenv.hostPlatform.isDarwin then
  macBuild
else if stdenv.hostPlatform.isLinux then
  linuxRendererBuild
else
  throw "retroarch.nix: unsupported platform ${stdenv.hostPlatform.system} (darwin and linux only)"
