# semu_app.nix — the composed bundle `make setup` links at
# build/nix/result: the CLI, every contract-required emulator
# package, ES-DE, the bezels.json/shaders.json visual assets, and the runtime tools.
#
# The composed bundle root is the one meaning of SEMU_ASSET_ROOT:
# <root>/bin contains executables, <root>/lib/semu contains Semu runtime
# helpers, and <root>/share/{semu,libretro} contains visual assets. lib/semu is
# materialized because symlinkJoin would otherwise link it wholesale to the
# CLI store path, preventing other packaged runtime helpers from being merged.
{ lib, symlinkJoin, makeWrapper, jq
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
  nativeBuildInputs = [ makeWrapper jq ];
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

    # Match the modes Nix assigns when the output enters the immutable store.
    # The contract must describe the copied runtime, not the writable build tree.
    chmod -R a-w "$out/bin" "$out/lib" "$out/share"
    exportContractRoot="$(mktemp -d)"
    for namespace in bin lib share; do
      cp -RL --preserve=mode,timestamps \
        "$out/$namespace" "$exportContractRoot/$namespace"
    done
    binDigest="$("$out/lib/semu/semu-btrc" package appimage \
      payload-identity --root "$exportContractRoot/bin")"
    libDigest="$("$out/lib/semu/semu-btrc" package appimage \
      payload-identity --root "$exportContractRoot/lib")"
    shareDigest="$("$out/lib/semu/semu-btrc" package appimage \
      payload-identity --root "$exportContractRoot/share")"
    for digest in "$binDigest" "$libDigest" "$shareDigest"; do
      test "''${#digest}" = 64
      case "$digest" in
        *[!0-9a-f]*) exit 1 ;;
      esac
    done
    exportContractFile="$(mktemp)"
    jq --sort-keys --null-input \
      --arg bin "$binDigest" \
      --arg lib "$libDigest" \
      --arg share "$shareDigest" \
      '{
        schema_version: 1,
        copy_policy: "recursive-dereference-preserve-mode",
        nodes: [
          {path: "bin", sha256: $bin},
          {path: "lib", sha256: $lib},
          {path: "share", sha256: $share}
        ]
      }' > "$exportContractFile"
    chmod u+w "$out/share/semu"
    install -m 0444 "$exportContractFile" \
      "$out/share/semu/runtime-export-contract.json"
    chmod a-w "$out/share/semu"
    rm -f "$exportContractFile"
    chmod -R u+w "$exportContractRoot"
    rm -rf "$exportContractRoot"
  '';
  meta.description = "Semu with all contract-required emulators and assets bundled";
}
