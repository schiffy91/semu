# The C that btrcpy makes of the two programs an emulator build takes in: the loader it links and the
# bridge RetroArch compiles into its gl3 driver. Both are committed (loader/generated, retroarch/generated)
# and the emulators build from the committed files, so the compiler is no input of any emulator. This
# derivation makes them afresh; the check `generated-c` fails when the committed files are not these.
{ lib, stdenv, btrcpy, writeText, rendererRoot }:

let rendererHeader = import (rendererRoot + "/semu_render_header.nix") { inherit writeText; };
in stdenv.mkDerivation {
  name = "semu-generated-c";
  src = lib.fileset.toSource {
    root = rendererRoot;
    fileset = lib.fileset.unions [
      (lib.fileset.fileFilter (file: !file.hasExt "c") (rendererRoot + "/loader"))
      (lib.fileset.fileFilter (file: !file.hasExt "c") (rendererRoot + "/retroarch"))
    ];
  };
  dontConfigure = true;
  nativeBuildInputs = [ btrcpy ];
  buildPhase = ''
    export HOME="$TMPDIR"  # btrcpy writes its cache under $HOME
    install -Dm644 ${rendererHeader} include/semu_renderer.h
    btrcpy loader/semu_renderer_loader.btrc -o semu_renderer_loader.c --strict-imports --no-cache --no-stdlib --no-dce
    CPATH="$PWD/include" btrcpy retroarch/runtime_bridge.btrc -o semu_retroarch.c --strict-imports --no-cache --no-stdlib --no-dce
    # A header beside the source is named by the directory the build stood in; name it by itself.
    sed -E -i 's|^#include "/.*/([^/"]+\.h)"$|#include "\1"|' semu_renderer_loader.c semu_retroarch.c
    ! grep -n '^#include "/' semu_renderer_loader.c semu_retroarch.c
  '';
  installPhase = ''
    install -Dm644 semu_renderer_loader.c "$out/semu_renderer_loader.c"
    install -Dm644 semu_retroarch.c "$out/semu_retroarch.c"
  '';
}
