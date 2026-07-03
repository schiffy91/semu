# ryujinx.nix — Ryujinx (the Ryubing continuation fork) BUILT FROM SOURCE,
# owned next to its contract (emulator.json declares the macos native backend
# this satisfies). A .NET 9 app (global.json wants sdk 9.0.100,
# rollForward latestFeature): buildDotnetModule restores from the pinned
# sibling deps.json (regenerate with the fetch-deps passthru:
#   nix build .#ryujinx.fetch-deps && ./result src/semu/emulators/ryujinx/deps.json
# ) and publishes the src/Ryujinx desktop project.
#
# The launch contract (emulator.json macos.executable) addresses
# ~/.local/share/ryujinx-app/Ryujinx.app/Contents/MacOS/Ryujinx, so the
# install phase assembles a Ryujinx.app bundle (Info.plist + icon from the
# source's distribution/macos, executable shimmed to the wrapped store
# binary) and bin/ryujinx stages it into that home path — re-staged whenever
# the store path changes, so rebuilds propagate.
{ lib
, stdenv
, buildDotnetModule
, dotnetCorePackages
, fetchgit
, cctools
, darwin
, ffmpeg
, openal
, libsoundio
, sndio
, SDL2
, SDL2_mixer
, vulkan-loader
, moltenvk
, glew
, libGL
, libX11
, libICE
, libSM
, libXcursor
, libXext
, libXi
, libXrandr
, udev
, pulseaudio
}:

buildDotnetModule rec {
  pname = "ryujinx";
  version = "1.3.3";

  src = fetchgit {
    url = "https://git.ryujinx.app/ryubing/ryujinx.git";
    # tag 1.3.3, resolved 2026-07-02 via git ls-remote (pin the commit, not
    # the movable tag)
    rev = "e2143d43bcb6762340d8a01f20e7b5fdf104f02f";
    hash = "sha256-LhQaXxmj5HIgfmrsDN8GhhVXlXHpDO2Q8JtNLaCq0mk=";
  };

  # Ryujinx.csproj's OSX PostBuild target ad-hoc codesigns the apphost.
  nativeBuildInputs = lib.optionals stdenv.hostPlatform.isDarwin [
    cctools
    darwin.sigtool
  ];

  # parallel msbuild races the generators (same workaround nixpkgs' ryubing
  # recipe carries)
  enableParallelBuilding = false;

  dotnet-sdk = dotnetCorePackages.sdk_9_0;
  dotnet-runtime = dotnetCorePackages.runtime_9_0;

  nugetDeps = ./deps.json;

  projectFile = "src/Ryujinx/Ryujinx.csproj";

  # the set nixpkgs' ryubing recipe proved out: audio backends, Vulkan, and
  # the Avalonia UI's windowing libraries
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
  ] ++ lib.optionals stdenv.hostPlatform.isLinux [
    udev
    pulseaudio
  ] ++ lib.optionals stdenv.hostPlatform.isDarwin [
    moltenvk
  ];

  # never self-update out from under the store; keep state in the OS config
  # dir (~/Library/Application Support/Ryujinx — an emulator.json sandbox
  # grant) instead of portable mode
  dotnetFlags = [
    "/p:ExtraDefineConstants=DISABLE_UPDATER%2CFORCE_EXTERNAL_BASE_DIR"
  ];

  executables = [ "Ryujinx" ];

  # macOS 26's system libicucore and the nixpkgs ICU that
  # System.Globalization.Native loads collide inside one process and SIGABRT
  # in icu::Locale (even `dotnet --version` dies). Invariant globalization
  # sidesteps ICU for the build-time dotnet and the launched app alike;
  # Ryujinx ships its own locale catalog, so no user-facing cultures depend
  # on it.
  env = lib.optionalAttrs stdenv.hostPlatform.isDarwin {
    DOTNET_SYSTEM_GLOBALIZATION_INVARIANT = "1";
  };
  makeWrapperArgs = lib.optionals stdenv.hostPlatform.isDarwin [
    "--set DOTNET_SYSTEM_GLOBALIZATION_INVARIANT 1"
  ];

  postFixup = lib.optionalString stdenv.hostPlatform.isDarwin ''
    bundle=$out/Applications/Ryujinx.app
    mkdir -p $bundle/Contents/MacOS $bundle/Contents/Resources
    cp ${src}/distribution/macos/Info.plist $bundle/Contents/
    cp ${src}/distribution/macos/Ryujinx.icns $bundle/Contents/Resources/
    echo -n "APPL????" > $bundle/Contents/PkgInfo
    cat > $bundle/Contents/MacOS/Ryujinx <<SHIM
    #!/bin/bash
    exec "$out/bin/Ryujinx" "\$@"
    SHIM
    chmod +x $bundle/Contents/MacOS/Ryujinx

    cat > $out/bin/ryujinx <<WRAPPER
    #!/bin/bash
    APP_DIR="\$HOME/.local/share/ryujinx-app"
    if [ "\$(cat "\$APP_DIR/.nix-source" 2>/dev/null)" != "$out" ]; then
      echo "Staging Ryujinx into \$APP_DIR..."
      mkdir -p "\$APP_DIR"
      rm -rf "\$APP_DIR/Ryujinx.app"
      cp -R "$out/Applications/Ryujinx.app" "\$APP_DIR/"
      chmod -R u+w "\$APP_DIR/Ryujinx.app"
      echo "$out" > "\$APP_DIR/.nix-source"
    fi
    open "\$APP_DIR/Ryujinx.app" --args "\$@"
    WRAPPER
    chmod +x $out/bin/ryujinx
  '';

  meta = {
    description = "Nintendo Switch emulator (Ryubing fork), built from source";
    homepage = "https://git.ryujinx.app/ryubing/ryujinx";
    changelog = "https://git.ryujinx.app/ryubing/ryujinx/-/blob/${version}/CHANGELOG.md";
    platforms = [ "aarch64-darwin" "x86_64-darwin" "x86_64-linux" "aarch64-linux" ];
    license = lib.licenses.mit;
    mainProgram = "Ryujinx";
  };
}
