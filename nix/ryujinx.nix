{ lib, stdenv, fetchurl, autoPatchelfHook ? null, makeWrapper ? null,
  gtk3 ? null, libX11 ? null, libgbm ? null, mesa ? null, SDL2 ? null,
  openal ? null, icu ? null, openssl ? null, zlib ? null,
}:

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
  stdenv.mkDerivation {
    pname = "ryujinx";
    inherit version src;
    sourceRoot = ".";
    installPhase = ''
      mkdir -p $out/Applications
      cp -r Ryujinx.app $out/Applications/
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
    nativeBuildInputs = [ autoPatchelfHook makeWrapper ];
    buildInputs = [ gtk3 libX11 libgbm mesa SDL2 openal icu openssl zlib ];
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
