# PCSX2 is built from Semu's immutable source pin on Linux. package.json owns
# package identity; rendering.json owns the two direct renderer boundaries.
{
  lib,
  stdenv,
  fetchFromGitHub,
  pcsx2,
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

  source =
    (fetchFromGitHub {
      inherit (sourceContract) owner repo;
      rev = sourceContract.revision;
      hash = sourceContract.sha256;
    })
    // {
      # nixpkgs reads src.tag only to stamp the version. Source identity still
      # comes exclusively from the immutable commit and hash above.
      tag = "v${packageContract.version}";
    };

  semuPatch = rendering.patch;
  semuPatchText = rendering.patchText;
  semuPatchHash = rendering.patchHash;
  packagePatch = builtins.elemAt packageContract.patches 0;
  systemCubebPatch = ./. + "/${packagePatch.file}";
  systemCubebPatchHash = builtins.hashFile "sha256" systemCubebPatch;

  buildContract = {
    schema_version = packageContract.schema_version;
    build_kind = packageContract.build_kind;
    version = packageContract.version;
    source_owner = sourceContract.owner;
    source_repo = sourceContract.repo;
    source_revision = sourceContract.revision;
    source_sha256 = sourceContract.sha256;
    package_patches = [
      {
        file = packagePatch.file;
        sha256 = systemCubebPatchHash;
      }
    ];
    patch_sha256 = semuPatchHash;
    executable = outputContract.main_program;
    linked_executable = outputContract.linked_program;
    render_hook = renderHook;
  };
  buildContractJson = builtins.toJSON buildContract;

  sourceBuild =
    assert lib.assertMsg (
      packageContract.schema_version == 1
      && packageContract.id == "pcsx2"
      && packageContract.build_kind == "source"
      && packageContract.platforms == [ "x86_64-linux" ]
      && packageContract.graphics_apis == [ "opengl" ]
    ) "PCSX2 must remain an x86_64-linux OpenGL source build";
    assert lib.assertMsg (
      sourceContract.kind == "github"
      && builtins.match "^[0-9a-f]{40}$" sourceContract.revision != null
      && lib.hasPrefix "sha256-" sourceContract.sha256
    ) "PCSX2's Linux source contract must be an immutable GitHub revision";
    assert lib.assertMsg (
      builtins.length packageContract.patches == 1
      && packagePatch.file == "system-cubeb.patch"
      && packagePatch.sha256 == systemCubebPatchHash
    ) "PCSX2's package patch must be exact and hash-bound";
    assert lib.assertMsg (
      !packageContract.fallbacks.linux_host_executable
      && !packageContract.fallbacks.linux_flatpak
      && !packageContract.fallbacks.linux_prebuilt_binary
    ) "PCSX2's Linux package contract must reject every runtime fallback";
    assert lib.assertMsg (
      renderHook.abi == 2
      && renderHook.linkage == "direct"
      && gamePhase.source == postUiPhase.source
      && lib.hasInfix "#include <semu_renderer.h>" semuPatchText
      && lib.hasInfix "semu_render_game_gl(&s_semu_frame);" semuPatchText
      && lib.hasInfix "semu_render_post_ui_gl(&s_semu_frame);" semuPatchText
      && lib.hasInfix "s_semu_frame.color_buffer = GL_BACK;" semuPatchText
      && lib.hasInfix "surfaces[0].rotation = 0;" semuPatchText
    ) "PCSX2's patch must carry the direct two-phase ABI 2 OpenGL renderer";
    pcsx2.overrideAttrs (
      previous:
      let
        previousEnvironment = previous.env or { };
      in
      {
        pname = packageContract.id;
        version = packageContract.version;
        src = source;
        patches = [ systemCubebPatch semuPatch ];
        patchFlags = [ "-p1" "--fuzz=0" ];
        buildInputs = lib.unique ((previous.buildInputs or [ ]) ++ [ semuRenderer ]);

        postPatch = (previous.postPatch or "") + ''
          test -s ${semuRenderer}/include/${renderHook.header}
          renderer=${lib.escapeShellArg gamePhase.source}

          test "$(grep -Fc '#include <semu_renderer.h>' "$renderer")" -eq 1
          test "$(grep -Fc 'semu_render_game_gl(&s_semu_frame);' "$renderer")" -eq 1
          test "$(grep -Fc 'semu_render_post_ui_gl(&s_semu_frame);' "$renderer")" -eq 1
          grep -Fq 's_semu_frame.struct_size = sizeof(s_semu_frame);' "$renderer"
          grep -Fq 's_semu_frame.presentation_aspect' "$renderer"
          grep -Fq 'SEMU_RENDER_LAYOUT_VARIANT_B' "$renderer"
          grep -Fq 's_semu_frame.color_buffer = GL_BACK;' "$renderer"
          grep -Fq 's_semu_frame.surfaces[0].rotation = 0;' "$renderer"
          grep -Fq 's_semu_context->GetProcAddress(name)' "$renderer"
          grep -Fq 's_semu_context->IsCurrent()' "$renderer"

          draw_line="$(grep -nF 'DrawStretchRect(flip_sr, dRect, ds);' "$renderer" | cut -d: -f1)"
          game_line="$(grep -nF 'semu_render_game_gl(&s_semu_frame);' "$renderer" | cut -d: -f1)"
          ui_line="$(grep -nF 'RenderImGui();' "$renderer" | cut -d: -f1)"
          post_line="$(grep -nF 'semu_render_post_ui_gl(&s_semu_frame);' "$renderer" | cut -d: -f1)"
          swap_line="$(awk '/m_gl_context->SwapBuffers\(\);/ { print NR; exit }' "$renderer")"
          test -n "$draw_line" -a -n "$game_line" -a -n "$ui_line"
          test "$draw_line" -lt "$game_line"
          test "$ui_line" -lt "$post_line"
          test "$post_line" -lt "$swap_line"
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
          mainProgram = "pcsx2-qt";
          platforms = [ "x86_64-linux" ];
        };
      }
    );
in
assert lib.assertMsg (lib.elem stdenv.hostPlatform.system packageContract.platforms)
  "Semu PCSX2 supports only the package.json platform set";
sourceBuild
