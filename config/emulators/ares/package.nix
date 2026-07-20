# Exact macOS ares source slice. package.json owns package identity while
# rendering.json owns the renderer ABI and hook boundaries.
{
  lib,
  stdenv,
  fetchzip,
  ares,
  semuRenderer,
}:

let
  packageContract = builtins.fromJSON (builtins.readFile ./package.json);
  rendering = (import ../../../packaging/nix/render-hook.nix { inherit lib; }) {
    emulatorDir = ./.;
  };
  sourceContract = packageContract.source;
  coreContract = packageContract.cores;
  outputContract = packageContract.outputs;
  renderHook = rendering.hook;
  gamePhase = rendering.gamePhase;
  postUiPhase = rendering.postUiPhase;
  driverBuild = renderHook.driver_build;
  metadata = packageContract.metadata;

  sourceArchive = fetchzip {
    url = sourceContract.url;
    hash = sourceContract.sha256;
    stripRoot = false;
  };

  semuPatch = rendering.patch;
  semuPatchText = rendering.patchText;
  semuPatchHash = rendering.patchHash;

  buildContract = {
    schema_version = packageContract.schema_version;
    build_kind = packageContract.build_kind;
    version = packageContract.version;
    source_owner = sourceContract.owner;
    source_repo = sourceContract.repo;
    source_tag = sourceContract.tag;
    source_revision = sourceContract.revision;
    source_url = sourceContract.url;
    source_sha256 = sourceContract.sha256;
    cores = coreContract;
    patch_sha256 = semuPatchHash;
    executable = outputContract.main_program;
    render_hook = renderHook;
  };
  buildContractJson = builtins.toJSON buildContract;

  sourceBuild =
    assert lib.assertMsg (
      packageContract.schema_version == 1
      && packageContract.id == "ares"
      && packageContract.build_kind == "source"
      &&
        packageContract.platforms == [
          "aarch64-darwin"
          "x86_64-darwin"
        ]
      && packageContract.graphics_apis == [ "opengl" ]
    ) "ares must remain an exact macOS OpenGL source build";
    assert lib.assertMsg (
      sourceContract.kind == "upstream_release_archive"
      && builtins.match "^[0-9a-f]{40}$" sourceContract.revision != null
      && lib.hasPrefix "sha256-" sourceContract.sha256
      && lib.hasInfix sourceContract.tag sourceContract.url
    ) "ares must use its immutable hash-pinned upstream source archive";
    assert lib.assertMsg (
      sourceContract.tag == "v${packageContract.version}"
    ) "ares source tag and package version diverge";
    assert lib.assertMsg (
      !packageContract.fallbacks.host_executable
      && !packageContract.fallbacks.flatpak
      && !packageContract.fallbacks.libretro
      && !packageContract.fallbacks.prebuilt_binary
    ) "ares must reject host, Flatpak, libretro, and prebuilt fallbacks";
    assert lib.assertMsg (
      coreContract == [ "n64" ]
    ) "the Semu ares slice must compile only the declared n64 core";
    assert lib.assertMsg (
      renderHook.abi == 2
      && renderHook.linkage == "direct"
      && lib.hasInfix "semu_render_game_gl(&semuFrame);" semuPatchText
      && lib.hasInfix "semu_render_post_ui_gl(&semuFrame);" semuPatchText
      && !lib.hasInfix "virtualPorts[0].pad.select" semuPatchText
      && !lib.hasInfix "virtualPorts[0].pad.start" semuPatchText
      && !lib.hasInfix "program.quit();" semuPatchText
    ) "ares patch must carry only the two declared direct renderer phases";
    ares.overrideAttrs (
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
        cmakeFlags = (previous.cmakeFlags or [ ]) ++ [
          "-DARES_CORES=${lib.concatStringsSep ";" coreContract}"
        ];

        postPatch = (previous.postPatch or "") + ''
          identity=${lib.escapeShellArg sourceContract.identity_file}
          test -s "$identity"
          grep -Fq '"describe": "${sourceContract.tag}"' "$identity"
          grep -Fq '"hash": "${sourceContract.revision_short}"' "$identity"

          test -s ${semuRenderer}/include/${renderHook.header}
          game_source=${lib.escapeShellArg gamePhase.source}
          post_ui_source=${lib.escapeShellArg postUiPhase.source}
          driver_cmake=${lib.escapeShellArg driverBuild.cmake_source}
          driver_registry=${lib.escapeShellArg driverBuild.registry_source}
          game_render_line="$(grep -nF 'render(sources[0].width, sources[0].height' \
            "$game_source" | cut -d: -f1)"
          game_call_line="$(grep -nF 'semu_render_game_gl(&semuFrame);' \
            "$game_source" | cut -d: -f1)"
          output_line="$(grep -nF 'OpenGL::output();' "$post_ui_source" | cut -d: -f1)"
          post_ui_line="$(grep -nF 'semu_render_post_ui_gl(&semuFrame);' \
            "$post_ui_source" | cut -d: -f1)"
          flush_line="$(grep -nF '[[view openGLContext] flushBuffer];' \
            "$post_ui_source" | awk -F: -v post="$post_ui_line" \
            '$1 > post { print $1; exit }')"
          test "$game_render_line" -lt "$game_call_line"
          test "$output_line" -lt "$post_ui_line"
          test "$flush_line" -eq "$((post_ui_line + 2))"
          grep -Fq 'SEMU_RENDER_ORIGIN_BOTTOM_LEFT' "$game_source"
          grep -Fq 'semuFrame.presentation_aspect = (float)targetWidth / (float)targetHeight;' \
            "$game_source"
          grep -Fq 'semuFrame.layout_variant = SEMU_RENDER_LAYOUT_AUTO;' "$game_source"
          grep -Fq ${lib.escapeShellArg driverBuild.feature} "$driver_cmake"
          grep -Fq 'video/cgl.cpp' "$driver_cmake"
          grep -Fq ${lib.escapeShellArg driverBuild.feature} "$driver_registry"
          grep -Fq ${lib.escapeShellArg driverBuild.driver} "$driver_registry"
        '';

        env = previousEnvironment // {
          NIX_CFLAGS_COMPILE =
            (previousEnvironment.NIX_CFLAGS_COMPILE or "")
            + " -D${renderHook.compile_define} -I${semuRenderer}/include";
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
          executable="$out/${outputContract.main_program}"
          contract="$out/${outputContract.build_contract}"
          test -x "$executable"
          test -s "$contract"
          test "$(cat "$contract")" = ${lib.escapeShellArg buildContractJson}
          grep -aFq '${gamePhase.symbol}' "$executable"
          grep -aFq '${postUiPhase.symbol}' "$executable"
          otool -L "$executable" | grep -Fq 'lib${renderHook.library}.dylib'
          otool -l "$executable" | grep -Fq '${semuRenderer}/lib'
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
  "Semu ares supports only the package.json macOS platform set";
sourceBuild
