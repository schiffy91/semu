#!/usr/bin/env bash
# Writes the C the compiler makes of the loader and the RetroArch bridge over the committed copies the
# emulators build from. Run it after changing either source, or when the check `generated-c` says the
# copies are behind; then update the locks that name src/renderer and commit both.
set -eu
cd "$(dirname "$0")/.."
made=$(nix build --no-link --print-out-paths "path:$PWD/src/renderer#generated")
install -m644 "$made/semu_renderer_loader.c" src/renderer/loader/generated/semu_renderer_loader.c
install -m644 "$made/semu_retroarch.c" src/renderer/retroarch/generated/semu_retroarch.c
git status --short src/renderer/loader/generated src/renderer/retroarch/generated
