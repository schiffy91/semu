# ES-DE on Linux, run on the bundle's glibc. ES-DE keeps an older nixpkgs (the last one with
# FreeImage), so it links an older glibc than everything else in the bundle. The GPU drivers it
# loads at run time (the release's Mesa, or a NixOS host's /run/opengl-driver) come from the
# current nixpkgs and need the newer glibc, which a process started on the older one cannot load:
# ES-DE got no OpenGL and no window. glibc and libstdc++ are backward compatible, so this copy
# points ES-DE's binaries at the current ones (interpreter and front of the runpath); its other
# libraries stay where they are. share/ is linked, since ES-DE finds its resources beside its binary.
# SDL comes first from the current nixpkgs too: on the old pin's sdl2-compat 2.32.56 over SDL3 3.2.20,
# ES-DE's main thread deadlocked in SDL's audio device lock on the Deck (a stutter on every move
# through a list, then a freeze), whichever audio backend SDL used.
{ runCommand, patchelf, glibc, stdenv, SDL2, esDe }:

runCommand "es-de-semu-${esDe.version}" { nativeBuildInputs = [ patchelf ]; passthru = { inherit (esDe) version; }; } ''
  mkdir -p "$out/bin"
  ln -s ${esDe}/share "$out/share"
  for program in ${esDe}/bin/*; do
    name="$(basename "$program")"
    cp "$program" "$out/bin/$name"
    chmod u+w "$out/bin/$name"
    patchelf --set-interpreter ${glibc}/lib/ld-linux-x86-64.so.2 \
      --set-rpath "${glibc}/lib:${stdenv.cc.cc.lib}/lib:${SDL2}/lib:$(patchelf --print-rpath "$program")" "$out/bin/$name"
  done
  HOME="$TMPDIR" "$out/bin/es-de" --help > /dev/null  # starts on the new loader without a display
''
