# What an emulator links instead of the renderer: the three ABI 3 entry points forwarding to
# libsemurenderer.so at run time. Built from the loader's committed C and the header alone, so neither
# a renderer change nor a new compiler changes this derivation or the emulators that link it.
# loader/generated/semu_renderer_loader.c is what btrcpy makes of loader/semu_renderer_loader.btrc;
# generated.nix makes it afresh and the check `generated-c` holds the two together.
{ lib, stdenv, writeText, rendererRoot }:

let
  loaderSource = lib.fileset.toSource {
    root = rendererRoot;
    fileset = lib.fileset.unions [ (rendererRoot + "/loader/generated") (rendererRoot + "/loader/dlfcn_abi.h") ];
  };
  rendererHeader = import (rendererRoot + "/semu_render_header.nix") { inherit writeText; };
in
stdenv.mkDerivation {
  pname = "semu-renderer-loader";
  version = "3";
  src = loaderSource;
  dontConfigure = true;
  allowSubstitutes = false;  # compiled by Semu, never a cache binary
  buildPhase = ''
    $CC -c loader/generated/semu_renderer_loader.c -Iloader -o semu_renderer_loader.o -std=c11 -O2 -fPIC -Wall -Wno-unused-function
    $AR rcs libsemurendererloader.a semu_renderer_loader.o
  '';
  installPhase = ''
    install -Dm644 ${rendererHeader} "$out/include/semu_renderer.h"
    install -Dm644 libsemurendererloader.a "$out/lib/libsemurendererloader.a"
  '';
  meta.description = "Run-time loader for libsemurenderer (link with -lsemurendererloader -ldl)";
}
