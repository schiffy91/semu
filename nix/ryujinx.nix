{ lib, stdenv, fetchurl, makeWrapper, writeShellScript }:

let
  version = "1.3.3";
  sources = {
    aarch64-darwin = {
      url = "https://git.ryujinx.app/api/v4/projects/1/packages/generic/Ryubing/${version}/ryujinx-${version}-macos_universal.app.tar.gz";
      hash = "sha256-5IGLuEyY4NMSBpGCHpB3IJnkYQEnPT8UX/2xDu4sDbs=";
    };
    x86_64-darwin = {
      url = "https://git.ryujinx.app/api/v4/projects/1/packages/generic/Ryubing/${version}/ryujinx-${version}-macos_universal.app.tar.gz";
      hash = "sha256-5IGLuEyY4NMSBpGCHpB3IJnkYQEnPT8UX/2xDu4sDbs=";
    };
    x86_64-linux = {
      url = "https://git.ryujinx.app/api/v4/projects/1/packages/generic/Ryubing/${version}/ryujinx-${version}-linux_x64.tar.gz";
      hash = "sha256-GbZ7Iicm8o0RhG6bfLrtET6gPCvgFkCGYv+yfFWL0ow=";
    };
    aarch64-linux = {
      url = "https://git.ryujinx.app/api/v4/projects/1/packages/generic/Ryubing/${version}/ryujinx-${version}-linux_arm64.tar.gz";
      hash = "sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="; # TODO: get hash
    };
  };
  src = fetchurl (sources.${stdenv.hostPlatform.system} or (throw "Ryujinx: unsupported platform ${stdenv.hostPlatform.system}"));
in
if stdenv.hostPlatform.isDarwin then
  # .NET apps need a writable directory. We store the tarball and extract
  # to ~/.local/share/ryujinx-app on first launch via a wrapper script.
  stdenv.mkDerivation {
    pname = "ryujinx";
    inherit version src;
    sourceRoot = ".";
    nativeBuildInputs = [ makeWrapper ];
    installPhase = ''
      mkdir -p $out/share/ryujinx $out/bin

      # Store the original tarball for extraction at runtime
      cp ${src} $out/share/ryujinx/ryujinx.tar.gz

      # Wrapper that extracts to a writable location on first run
      cat > $out/bin/ryujinx <<'WRAPPER'
      #!/bin/bash
      APP_DIR="$HOME/.local/share/ryujinx-app"
      if [ ! -d "$APP_DIR/Ryujinx.app" ]; then
        echo "First run: extracting Ryujinx..."
        mkdir -p "$APP_DIR"
        tar xzf "$0/../share/ryujinx/ryujinx.tar.gz" -C "$APP_DIR"
        xattr -dr com.apple.quarantine "$APP_DIR/Ryujinx.app" 2>/dev/null
      fi
      open "$APP_DIR/Ryujinx.app" --args "$@"
      WRAPPER
      chmod +x $out/bin/ryujinx

      # Fix the share path in the wrapper (make it relative to the script)
      substituteInPlace $out/bin/ryujinx \
        --replace '$0/../share/ryujinx/ryujinx.tar.gz' "$out/share/ryujinx/ryujinx.tar.gz"
    '';
    meta = {
      description = "Nintendo Switch emulator (Ryubing fork)";
      homepage = "https://git.ryujinx.app/ryubing/ryujinx";
      platforms = [ "aarch64-darwin" "x86_64-darwin" ];
      license = lib.licenses.mit;
    };
  }
else
  stdenv.mkDerivation {
    pname = "ryujinx";
    inherit version src;
    sourceRoot = ".";
    nativeBuildInputs = [ makeWrapper ];
    installPhase = ''
      mkdir -p $out/bin $out/lib/ryujinx
      cp -r publish/* $out/lib/ryujinx/
      makeWrapper $out/lib/ryujinx/Ryujinx $out/bin/ryujinx
    '';
    meta = {
      description = "Nintendo Switch emulator (Ryubing fork)";
      homepage = "https://git.ryujinx.app/ryubing/ryujinx";
      platforms = [ "x86_64-linux" "aarch64-linux" ];
      license = lib.licenses.mit;
    };
  }
