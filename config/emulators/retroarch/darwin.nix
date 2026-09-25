# RetroArch for macOS from Semu's pinned source: the make build (nixpkgs marks retroarch-bare
# broken on darwin), OpenGL core for the Semu render hook beside Metal, the same command and
# hook patches as Linux, and the renderer reached through the loader. `make install` leaves a
# bare Mach-O whose Cocoa entry point hangs without its bundle, so the real RetroArch.app is
# assembled from the pinned source and bin/retroarch execs it.
{ lib, stdenv, pkg-config, unzip, zlib, btrcpy, semuRendererLoader, bridgeSource, source, version }:

let
  hooked = stdenv.mkDerivation {
    pname = "retroarch-semu";
    inherit version;
    src = source;
    allowSubstitutes = false;  # compiled by Semu, never a cache binary
    patches = [ ./retroarch.patch ./retroarch_commands.patch ./retroarch_get_status_null_safety.patch ./retroarch_darwin.patch ./retroarch_darwin_fullscreen.patch ./retroarch_darwin_runloop.patch ];
    patchFlags = [ "-p1" "--fuzz=0" ];  # a patch that drifted from the pinned source fails
    nativeBuildInputs = [ pkg-config unzip btrcpy ];
    buildInputs = [ zlib semuRendererLoader ];
    postPatch = ''
      btrcpy ${bridgeSource}/runtime_bridge.btrc -o gfx/semu_retroarch.c \
        --strict-imports --no-cache --no-stdlib --no-dce
      test -s gfx/semu_retroarch.c
      patchShebangs ./configure ./qb
    '';
    configureFlags = [
      "--enable-metal"
      "--enable-opengl_core"  # the gl3 driver the Semu renderer hooks
      "--enable-builtinglslang"
      "--disable-builtinzlib"
      "--disable-qt"
      "--disable-ffmpeg"
      "--disable-microphone"  # the make build lacks the CoreAudio microphone driver
      "--disable-update_cores"
      "--disable-update_assets"
      "--disable-update_core_info"
    ];
    env = {
      NIX_CFLAGS_COMPILE = "-DHAVE_SEMU_RENDERER -I${semuRendererLoader}/include";
      NIX_LDFLAGS = "-force_load ${semuRendererLoader}/lib/libsemurendererloader.a";  # the loader, never the renderer itself
    };
    enableParallelBuilding = true;
    postInstall = ''
      app="$out/Applications/RetroArch.app"
      mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
      mv "$out/bin/retroarch" "$app/Contents/MacOS/RetroArch"
      cp -R ${source}/pkg/apple/OSX/Resources/. "$app/Contents/Resources/"
      cp ${source}/pkg/apple/OSX/Info_Metal.plist "$app/Contents/Info.plist"
      substituteInPlace "$app/Contents/Info.plist" \
        --replace-fail '$(EXECUTABLE_NAME)' 'RetroArch' \
        --replace-fail '$(PRODUCT_BUNDLE_IDENTIFIER)' 'com.libretro.RetroArch' \
        --replace-fail \''${PRODUCT_NAME} 'RetroArch' \
        --replace-fail '$(MARKETING_VERSION)' '${version}' \
        --replace-fail '$(CURRENT_PROJECT_VERSION)' '${version}' \
        --replace-fail '$(MACOSX_DEPLOYMENT_TARGET)' '11.0'
      if grep -qE '\$\(|\$\{' "$app/Contents/Info.plist"; then echo "unexpanded Xcode variable in Info.plist" >&2; exit 1; fi
      test -s "$app/Contents/Resources/en.lproj/MainMenu_Metal.nib"
      grep -Fq semu-renderer-loader "$app/Contents/MacOS/RetroArch"  # the loader is linked in
      ! otool -L "$app/Contents/MacOS/RetroArch" | grep -Fq libsemurenderer  # loaded at run time, never linked
      cat > "$out/bin/retroarch" <<WRAPPER
      #!/bin/sh
      exec "$app/Contents/MacOS/RetroArch" "\$@"
      WRAPPER
      chmod +x "$out/bin/retroarch"
    '';
    meta.platforms = lib.platforms.darwin;
  };
in
hooked.overrideAttrs (previous: { passthru = (previous.passthru or { }) // { unwrapped = hooked; withCores = cores: hooked; }; })
