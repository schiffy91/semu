# Ryujinx is built from the immutable source contract beside this recipe.
# Linux statically names the shared renderer through managed P/Invoke; no
# runtime symbol discovery or native shim participates.
{
  lib,
  stdenv,
  buildDotnetModule,
  dotnetCorePackages,
  fetchzip,
  semuRenderer,
  cctools,
  darwin,
  ffmpeg,
  openal,
  libsoundio,
  sndio,
  SDL2,
  SDL2_mixer,
  vulkan-loader,
  moltenvk,
  glew,
  libGL,
  libX11,
  libICE,
  libSM,
  libXcursor,
  libXext,
  libXi,
  libXrandr,
  udev,
  pulseaudio,
}:

let
  packageContract = builtins.fromJSON (builtins.readFile ./package.json);
  rendering = (import ../../../packaging/nix/render-hook.nix { inherit lib; }) {
    emulatorDir = ./.;
  };
  sourceContract = packageContract.source;
  build = packageContract.build;
  outputContract = packageContract.outputs;
  renderHook = rendering.hook;
  isLinux = stdenv.hostPlatform.isLinux;
  isDarwin = stdenv.hostPlatform.isDarwin;

  source = fetchzip {
    url = sourceContract.url;
    hash = sourceContract.sha256;
  };

  semuPatch = rendering.patch;
  semuPatchHash = rendering.patchHash;
  addedPatchText = rendering.addedPatchText;
  gamePhase = rendering.gamePhase;
  postUiPhase = rendering.postUiPhase;
  compileProject = renderHook.compile_project;

  sourceEdits = lib.concatMapStringsSep "\n" (sourceKey: ''
    sed -i \
      -e '/<add key="${sourceKey}"/d' \
      -e '/<packageSource key="${sourceKey}">/,/<\/packageSource>/d' \
      ${lib.escapeShellArg build.nuget_config}
  '') build.dead_nuget_source_keys;
  packageReferenceEdits = lib.concatMapStringsSep "\n" (packageName: ''
    sed -i \
      -e '/PackageReference Include="${packageName}"/d' \
      ${lib.escapeShellArg build.project_file}
    sed -i \
      -e '/PackageVersion Include="${packageName}"/d' \
      ${lib.escapeShellArg build.central_packages}
  '') build.disabled_package_references;
  compileDefines = build.defines ++ lib.optionals isLinux build.linux_defines;
  dotnetExecutable = builtins.baseNameOf outputContract.linux_main_program;
  macosExecutable = builtins.baseNameOf outputContract.macos_main_program;
  macosIcon = builtins.baseNameOf build.macos_icon;
  executable =
    if isDarwin then outputContract.macos_main_program else outputContract.linux_main_program;

  semuBuildContract = {
    schema_version = packageContract.schema_version;
    id = packageContract.id;
    version = packageContract.version;
    build_kind = packageContract.build_kind;
    platform = stdenv.hostPlatform.system;
    source_url = sourceContract.url;
    source_revision = sourceContract.revision;
    source_sha256 = sourceContract.sha256;
    project_file = build.project_file;
    dotnet_sdk = build.dotnet_sdk;
    dotnet_runtime = build.dotnet_runtime;
    defines = compileDefines;
    executable = executable;
    patch_sha256 = semuPatchHash;
    render_hook = renderHook;
  };
  semuBuildContractJson = builtins.toJSON semuBuildContract;
in
assert lib.assertMsg (
  packageContract.build_kind == "source"
  && sourceContract.kind == "forgejo_archive"
  && builtins.match "^[0-9a-f]{40}$" sourceContract.revision != null
  && lib.hasPrefix "sha256-" sourceContract.sha256
  && lib.hasInfix sourceContract.revision sourceContract.url
  && lib.elem "x86_64-linux" packageContract.platforms
  && !packageContract.fallbacks.linux_host_executable
  && !packageContract.fallbacks.linux_flatpak
  && !packageContract.fallbacks.linux_prebuilt_binary
) "Ryujinx must remain an immutable source build without a Linux fallback";
assert lib.assertMsg (
  renderHook.abi == 2
  && renderHook.linkage == "direct_pinvoke"
  &&
    renderHook.platforms == [
      "x86_64-linux"
      "aarch64-linux"
    ]
  && lib.hasInfix "DllImport(LibraryName" addedPatchText
  && lib.hasInfix "private const string LibraryName = \"semurenderer\";" addedPatchText
  && lib.hasInfix "EntryPoint = \"semu_render_game_gl\"" addedPatchText
  && lib.hasInfix "EntryPoint = \"semu_render_post_ui_gl\"" addedPatchText
  && lib.hasInfix "private const uint RendererAbi = 2;" addedPatchText
  && lib.hasInfix "private const uint ColorBufferBack = 0x0405;" addedPatchText
  && lib.hasInfix "private const int FrameSize = 232;" addedPatchText
  && lib.hasInfix "private struct SemuRenderPointerMap" addedPatchText
  && lib.hasInfix "public SemuRenderPointerMap PointerMap;" addedPatchText
  && lib.hasInfix "public nint MapPointer;" addedPatchText
  && lib.hasInfix "AbiLayoutValid = HasExactAbiLayout();" addedPatchText
  && lib.hasInfix "if (!AbiLayoutValid" addedPatchText
  && lib.hasInfix "ColorBuffer = ColorBufferBack" addedPatchText
  && lib.hasInfix "Rotation = 0" addedPatchText
) "Ryujinx's patch must directly P/Invoke the two-phase ABI 2 renderer";

buildDotnetModule {
  pname = packageContract.id;
  version = packageContract.version;
  src = source;
  patches = [ semuPatch ];
  patchFlags = [
    "-p1"
    "--fuzz=0"
  ];

  nativeBuildInputs = lib.optionals isDarwin [
    cctools
    darwin.sigtool
  ];

  enableParallelBuilding = build.enable_parallel;

  # The release's updater packages were hosted on a retired project feed.
  # The package names, feed key, and replacement path all come from
  # package.json; this source build compiles the updater out permanently.
  postPatch = ''
    ${sourceEdits}
    ${packageReferenceEdits}

    cat > ${lib.escapeShellArg build.updater_stub_path} <<'STUB'
    using Gommon;
    using System;
    using System.Threading.Tasks;

    namespace Ryujinx.Ava.Systems
    {
        internal sealed class SemuStubVersionResponse
        {
            public string Version => string.Empty;
            public string ArtifactUrl => string.Empty;
            public string ReleaseUrlFormat => string.Empty;
        }

        internal static partial class Updater
        {
            private static SemuStubVersionResponse _versionResponse;

            public static Task<Optional<(Version Current, Version Incoming)>> CheckVersionAsync(bool showVersionUpToDate = false)
            {
                _versionResponse = null;
                return Task.FromResult<Optional<(Version Current, Version Incoming)>>(default);
            }
        }
    }
    STUB

    window=${lib.escapeShellArg gamePhase.source}
    embedded=${lib.escapeShellArg postUiPhase.source}
    opengl_project=${lib.escapeShellArg compileProject}
    grep -Fq 'ExtraDefineConstants' "$opengl_project"
    grep -Fq '$(DefineConstants);$(ExtraDefineConstants)' "$opengl_project"
    test "$(grep -Fc 'private const string LibraryName = "semurenderer";' "$window")" -eq 1
    test "$(grep -Fc 'EntryPoint = "semu_render_game_gl"' "$window")" -eq 1
    test "$(grep -Fc 'EntryPoint = "semu_render_post_ui_gl"' "$window")" -eq 1
    test "$(grep -Fc 'SemuRendererClient.RenderGame(' "$window")" -eq 1
    test "$(grep -Fc 'SemuRendererClient.RenderPostUi();' "$embedded")" -eq 1
    grep -Fq 'private const uint RendererAbi = 2;' "$window"
    grep -Fq 'private const uint ColorBufferBack = 0x0405;' "$window"
    grep -Fq 'private const int RenderSurfaceSize = 32;' "$window"
    grep -Fq 'private const int PointerMapSize = 96;' "$window"
    grep -Fq 'private const int FrameSize = 232;' "$window"
    grep -Fq 'private struct SemuRenderPointerSurface' "$window"
    grep -Fq 'private struct SemuRenderPointerMap' "$window"
    grep -Fq 'public SemuRenderPointerMap PointerMap;' "$window"
    grep -Fq 'public nint MapPointer;' "$window"
    grep -Fq 'Marshal.SizeOf<SemuRenderPointerMap>() == PointerMapSize' "$window"
    grep -Fq 'Marshal.SizeOf<SemuRenderFrameGl>() == FrameSize' "$window"
    grep -Fq 'nameof(SemuRenderFrameGl.PointerMap)) == 128' "$window"
    grep -Fq 'nameof(SemuRenderFrameGl.MapPointer)) == 224' "$window"
    grep -Fq 'AbiLayoutValid = HasExactAbiLayout();' "$window"
    grep -Fq 'if (!AbiLayoutValid || framebufferWidth <= 0 ||' "$window"
    grep -Fq 'ColorBuffer = ColorBufferBack' "$window"
    grep -Fq 'StructSize = (uint)FrameSize' "$window"
    grep -Fq 'PresentationAspect = (float)contentWidth / contentHeight' "$window"
    grep -Fq 'LayoutVariantB : LayoutDefault' "$window"
    grep -Fq 'Rotation = 0' "$window"
    grep -Fq 'Context.GetProcAddress' "$embedded"
    grep -Fq 'Context.IsCurrent ? Context.ContextHandle : nint.Zero' "$embedded"
    grep -Fq 'GL.BlitFramebuffer(' "$window"
    grep -Fq 'swapBuffersCallback();' "$window"
    grep -Fq 'Math.Min(dstX0, dstX1)' "$window"
    grep -Fq 'int nativeWidth = Math.Abs(srcX1 - srcX0);' "$window"

    blit_line="$(grep -nF 'GL.BlitFramebuffer(' "$window" | cut -d: -f1)"
    game_line="$(grep -nF 'SemuRendererClient.RenderGame(' "$window" | cut -d: -f1)"
    callback_line="$(grep -nF 'swapBuffersCallback();' "$window" | cut -d: -f1)"
    post_line="$(grep -nF 'SemuRendererClient.RenderPostUi();' "$embedded" | cut -d: -f1)"
    swap_line="$(grep -nF '_window?.SwapBuffers();' "$embedded" | cut -d: -f1)"
    test "$blit_line" -lt "$game_line"
    test "$game_line" -lt "$callback_line"
    test "$post_line" -lt "$swap_line"

  '';

  dotnet-sdk = builtins.getAttr build.dotnet_sdk dotnetCorePackages;
  dotnet-runtime = builtins.getAttr build.dotnet_runtime dotnetCorePackages;
  nugetDeps = ./. + "/${build.nuget_deps}";
  projectFile = build.project_file;

  runtimeDeps = [
    ffmpeg
    openal
    libsoundio
    sndio
    SDL2
    SDL2_mixer
    vulkan-loader
    glew
    libGL
    libX11
    libICE
    libSM
    libXcursor
    libXext
    libXi
    libXrandr
  ]
  ++ lib.optionals isLinux [
    semuRenderer
    udev
    pulseaudio
  ]
  ++ lib.optionals isDarwin [
    moltenvk
  ];

  dotnetFlags = [
    "/p:ExtraDefineConstants=${lib.concatStringsSep "%2C" compileDefines}"
  ];
  executables = build.executables;

  env = lib.optionalAttrs isDarwin {
    DOTNET_SYSTEM_GLOBALIZATION_INVARIANT = "1";
  };
  makeWrapperArgs = lib.optionals isDarwin [
    "--set DOTNET_SYSTEM_GLOBALIZATION_INVARIANT 1"
  ];

  postInstall = ''
    mkdir -p "$out/share/semu"
    printf '%s\n' ${lib.escapeShellArg semuBuildContractJson} \
      > "$out/${outputContract.build_contract}"
  '';

  postFixup = lib.optionalString isDarwin ''
    mkdir -p "$out/libexec"
    mv "$out/${outputContract.linux_main_program}" \
      "$out/libexec/${packageContract.id}-launch"

    bundle="$out/${build.macos_bundle}"
    mkdir -p "$bundle/Contents/MacOS" "$bundle/Contents/Resources"
    cp "${source}/${build.macos_info_plist}" "$bundle/Contents/Info.plist"
    cp "${source}/${build.macos_icon}" "$bundle/Contents/Resources/${macosIcon}"
    echo -n "APPL????" > "$bundle/Contents/PkgInfo"
    cat > "$bundle/Contents/MacOS/${macosExecutable}" <<SHIM
    #!/bin/bash
    exec "$out/libexec/${packageContract.id}-launch" "\$@"
    SHIM
    chmod +x "$bundle/Contents/MacOS/${macosExecutable}"

    cat > "$out/${outputContract.macos_wrapper}" <<WRAPPER
    #!/bin/bash
    exec "$out/${outputContract.macos_main_program}" "\$@"
    WRAPPER
    chmod +x "$out/${outputContract.macos_wrapper}"
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    runHook preInstallCheck
    test -x "$out/${executable}"
    contract="$out/${outputContract.build_contract}"
    test -s "$contract"
    test "$(cat "$contract")" = ${lib.escapeShellArg semuBuildContractJson}
    ${lib.optionalString isLinux ''
      test -s ${semuRenderer}/include/${renderHook.header}
      test -s ${semuRenderer}/lib/lib${renderHook.library}.so
      grep -R -aFq '${renderHook.library}' "$out/lib"
      grep -R -aFq '${gamePhase.symbol}' "$out/lib"
      grep -R -aFq '${postUiPhase.symbol}' "$out/lib"
      grep -aFq '${semuRenderer}/lib' "$out/${outputContract.linux_main_program}"
    ''}
    runHook postInstallCheck
  '';

  passthru = {
    semuBuildContract = semuBuildContract;
    semuRenderHook = renderHook;
    sourceRevision = sourceContract.revision;
  };

  meta = {
    inherit (packageContract.meta) description homepage;
    license = builtins.getAttr packageContract.meta.license lib.licenses;
    platforms = packageContract.platforms;
    mainProgram =
      if isDarwin then
        packageContract.meta.macos_main_program
      else
        packageContract.meta.linux_main_program;
  };
}
