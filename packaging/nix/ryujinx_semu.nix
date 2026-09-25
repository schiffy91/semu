# Ryujinx on macOS with Semu's Vulkan composition. Ryujinx loads the MoltenVK it bundles from its
# own folder (.NET resolves libMoltenVK.dylib there before any library path), so this folder puts
# Semu's stand-in under that name and keeps the original beside it as libMoltenVK.real.dylib, which
# the stand-in re-exports and forwards to. The folder is a real copy of the Ryujinx build, not
# links: .NET takes each assembly's folder from its real path, and linked assemblies led it back to
# the original MoltenVK.
{ runCommand, ryujinx, semuRenderer }:

runCommand "ryubing-semu-${ryujinx.version}" { passthru = { inherit (ryujinx) version; }; } ''
  app="$out/lib/ryubing"
  mkdir -p "$out/lib" "$out/bin"
  cp -R ${ryujinx}/lib/ryubing "$app"
  chmod -R u+w "$app"
  mv "$app/libMoltenVK.dylib" "$app/libMoltenVK.real.dylib"
  cp ${semuRenderer}/lib/semu-vulkan/beside/libMoltenVK.dylib "$app/libMoltenVK.dylib"
  sed "s|${ryujinx}/lib/ryubing|$app|g" ${ryujinx}/bin/Ryujinx > "$out/bin/Ryujinx"
  chmod +x "$out/bin/Ryujinx"
  grep -q "$app/Ryujinx" "$out/bin/Ryujinx"
''
