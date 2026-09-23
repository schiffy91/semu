# What an emulator links instead of the renderer: the three ABI 3 entry points forwarding to
# libsemurenderer.so at run time. Built from the loader and the header alone, so a renderer
# change never changes this derivation or the emulators that link it.
{ lib, stdenv, btrcpy, writeText, rendererRoot }:

let
  loaderSource = lib.fileset.toSource {
    root = rendererRoot;
    fileset = rendererRoot + "/loader/semu_renderer_loader.btrc";
  };
  rendererHeader = import (rendererRoot + "/semu_render_header.nix") { inherit writeText; };
in
stdenv.mkDerivation {
  pname = "semu-renderer-loader";
  version = "3";
  src = loaderSource;
  dontConfigure = true;
  allowSubstitutes = false;  # compiled by Semu, never a cache binary
  nativeBuildInputs = [ btrcpy ];
  buildPhase = ''
    btrcpy loader/semu_renderer_loader.btrc -o semu_renderer_loader.c --strict-imports --no-cache --no-stdlib --no-dce
    $CC -c semu_renderer_loader.c -o semu_renderer_loader.o -std=c11 -O2 -fPIC -Wall -Wno-unused-function
    $AR rcs libsemurendererloader.a semu_renderer_loader.o
  '';
  installPhase = ''
    install -Dm644 ${rendererHeader} "$out/include/semu_renderer.h"
    install -Dm644 libsemurendererloader.a "$out/lib/libsemurendererloader.a"
  '';
  meta.description = "Run-time loader for libsemurenderer (link with -lsemurendererloader -ldl)";
}
