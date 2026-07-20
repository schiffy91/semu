# semu_app.nix — the composed bundle `make setup` links at
# build/nix/result: the CLI, every contract-required emulator
# package, ES-DE, the bezels.json/shaders.json visual assets, and the runtime tools.
#
# The composed bundle root is the one meaning of SEMU_ASSET_ROOT:
# <root>/bin contains executables, <root>/lib/semu contains Semu runtime
# helpers, and <root>/share/{semu,libretro} contains visual assets. lib/semu is
# materialized because symlinkJoin would otherwise link it wholesale to the
# CLI store path, preventing other packaged runtime helpers from being merged.
{ lib, symlinkJoin, makeWrapper
, semuCli
, emulatorPackages ? [ ]
, esDe ? null
, semuBezels ? null
, semuShaders ? null
, runtimeTools ? [ ]
}:

let
  runtimePath = lib.makeBinPath runtimeTools;
in
symlinkJoin {
  name = "semu-app";
  paths = [ semuCli ] ++ emulatorPackages
    ++ lib.filter (x: x != null) [ esDe semuShaders ]
    ++ runtimeTools;
  nativeBuildInputs = [ makeWrapper ];
  postBuild = lib.optionalString (semuBezels != null) ''
    if [ -L "$out/lib/semu" ]; then
      cliLib="$(readlink "$out/lib/semu")"
      rm "$out/lib/semu"
      mkdir -p "$out/lib/semu"
      cp -RL "$cliLib/." "$out/lib/semu/"
    fi
    chmod -R u+w "$out/lib/semu"

    mkdir -p "$out/share"
    for staged in ${semuBezels}/share/*; do
      name="$(basename "$staged")"
      destination="$out/share/$name"
      # symlinkJoin may already have populated this namespace (notably
      # share/semu/retroarch-cores.txt). Merge bezel assets into it; replacing
      # the directory silently erased other packages' contract data.
      if [ -L "$destination" ]; then
        existing="$(readlink "$destination")"
        rm "$destination"
        mkdir -p "$destination"
        cp -RL "$existing/." "$destination/"
      else
        mkdir -p "$destination"
      fi
      cp -RL "$staged/." "$destination/"
    done
    if [ -d "${semuBezels}/lib/semu" ]; then
      cp -RL "${semuBezels}/lib/semu/." "$out/lib/semu/"
    fi
    chmod -R u+w "$out/lib/semu" "$out/share"

    rm "$out/bin/semu"
    makeWrapper "$out/lib/semu/semu-btrc" "$out/bin/semu" \
      --set SEMU_ASSET_ROOT "$out" \
      --set SEMU_BIN "$out/bin/semu" \
      --set SEMU_SOURCE_ROOT "$out/share/semu/config" \
      --prefix PATH : ${lib.escapeShellArg runtimePath}
  '';
  meta.description = "Semu with all contract-required emulators and assets bundled";
}
