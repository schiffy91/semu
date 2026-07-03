# ares.nix — the ares package recipe, owned next to its contract
# (emulator.json declares the macos native backend this satisfies). ares is
# the one emulator nixpkgs already packages correctly on Darwin
# (Applications/ares.app + bin/ares), so this is a passthrough kept only so
# every emulator's recipe lives at emulators/<id>/<id>.nix; the source pin
# is flake.lock's nixpkgs.
{ ares }:

ares
