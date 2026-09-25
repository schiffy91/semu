# Ryujinx on macOS with Semu's Vulkan composition. Ryujinx loads the MoltenVK it bundles from its
# own folder (.NET resolves libMoltenVK.dylib there before any library path), so this folder puts
# Semu's stand-in under that name and keeps the original beside it as libMoltenVK.real.dylib, which
# the stand-in re-exports and forwards to. Everything else links to the Ryujinx build; the launcher
# is a copy, because the .NET host takes its app folder from its own real path.
{ runCommand, ryujinx, semuRenderer }:

runCommand "ryubing-semu-${ryujinx.version}" { passthru = { inherit (ryujinx) version; }; } ''
  app="$out/lib/ryubing"
  mkdir -p "$app" "$out/bin"
  for file in ${ryujinx}/lib/ryubing/*; do ln -s "$file" "$app/"; done
  rm "$app/Ryujinx" "$app/libMoltenVK.dylib"
  cp ${ryujinx}/lib/ryubing/Ryujinx "$app/Ryujinx"
  ln -s ${ryujinx}/lib/ryubing/libMoltenVK.dylib "$app/libMoltenVK.real.dylib"
  cp ${semuRenderer}/lib/semu-vulkan/beside/libMoltenVK.dylib "$app/libMoltenVK.dylib"
  sed "s|${ryujinx}/lib/ryubing|$app|g" ${ryujinx}/bin/Ryujinx > "$out/bin/Ryujinx"
  chmod +x "$out/bin/Ryujinx"
  grep -q "$app/Ryujinx" "$out/bin/Ryujinx"
''
