# ES-DE for aarch64-darwin, built from the pinned source against Nix libraries
# instead of the upstream in-tree "external" dependency build, as an app bundle.
{ lib, stdenv, esDePackages, source }:

let
  version = "3.4.0";
  inherit (esDePackages) cmake gettext pkg-config curl ffmpeg freeimage freetype harfbuzz icu libgit2 poppler pugixml SDL2;
  app = "Applications/ES-DE.app/Contents";
in
assert lib.assertMsg (stdenv.hostPlatform.system == "aarch64-darwin") "ES-DE darwin recipe: only aarch64-darwin";
stdenv.mkDerivation {
  pname = "es-de";
  inherit version;

  src = source;  # the flake input pins the GitLab revision

  patches = [ ./settings-menu.patch ];  # SEMU SETTINGS entry in the main menu
  allowSubstitutes = false;  # never a cache binary

  # Route the macOS dependency branches to the generic Unix find_package ones.
  postPatch = ''
    substituteInPlace CMakeLists.txt \
      --replace-fail 'if(APPLE AND NOT IOS)' 'if(FALSE)' \
      --replace-fail $'elseif(APPLE)\n    set(COMMON_INCLUDE_DIRS' $'elseif(FALSE)\n    set(COMMON_INCLUDE_DIRS' \
      --replace-fail $'elseif(APPLE)\n    set(COMMON_LIBRARIES' $'elseif(FALSE)\n    set(COMMON_LIBRARIES'
    substituteInPlace es-pdf-converter/CMakeLists.txt \
      --replace-fail $'elseif(APPLE)\n    set(POPPLER_CPP_INCLUDE_DIR' $'elseif(FALSE)\n    set(POPPLER_CPP_INCLUDE_DIR'
    substituteInPlace locale/CMakeLists.txt \
      --replace-fail $'elseif(APPLE)\n    set(MSGFMT_BINARY ''${PROJECT_SOURCE_DIR}/external/local_install' $'elseif(FALSE)\n    set(MSGFMT_BINARY ''${PROJECT_SOURCE_DIR}/external/local_install'
    substituteInPlace CMake/Packages/FindPoppler.cmake \
      --replace-quiet 'GET_PREREQUISITES("''${POPPLER_LIBRARY}" POPPLER_PREREQS 1 0 "" "")' ""
  '';

  nativeBuildInputs = [ cmake gettext pkg-config ];
  buildInputs = [ curl ffmpeg freeimage freetype harfbuzz icu libgit2 poppler pugixml SDL2 ];

  cmakeFlags = [
    (lib.cmakeBool "APPLICATION_UPDATER" false)
    (lib.cmakeFeature "CMAKE_OSX_DEPLOYMENT_TARGET" "11.0")
  ];

  # Binaries and compiled locale catalogs land in the source root (EXECUTABLE_OUTPUT_PATH).
  installPhase = ''
    runHook preInstall
    root="$NIX_BUILD_TOP/$sourceRoot"
    mkdir -p "$out/${app}/MacOS" "$out/${app}/Resources/themes" "$out/bin" "$out/share/es-de"
    install -m755 "$root/ES-DE" "$out/${app}/MacOS/ES-DE"
    convert="$(find "$root" "$PWD" -name es-pdf-convert -type f -perm -u+x | head -n1)"
    test -n "$convert" && install -m755 "$convert" "$out/${app}/MacOS/es-pdf-convert"
    cp -R "$root/resources" "$out/${app}/Resources/resources"
    cp -R "$root/licenses" "$out/${app}/Resources/licenses"
    cp "$root/LICENSE" "$out/${app}/Resources/LICENSE"
    for theme in linear-es-de modern-es-de slate-es-de; do cp -R "$root/themes/$theme" "$out/${app}/Resources/themes/$theme"; done
    cp "$root/es-app/assets/ES-DE.icns" "$out/${app}/Resources/ES-DE.icns"
    sed -e 's/@ES_VERSION@/${version}/g' "$root/es-app/assets/ES-DE_Info.plist" > "$out/${app}/Info.plist"
    printf '#!/bin/sh\nexec "%s" "$@"\n' "$out/${app}/MacOS/ES-DE" > "$out/bin/es-de"  # not a symlink: ES-DE finds Resources beside its real path
    chmod 755 "$out/bin/es-de"
    ln -s "../../${app}/Resources/themes" "$out/share/es-de/themes"
    runHook postInstall
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    test -x "$out/bin/es-de"
    grep -Fq 'semu-settings-v2' "$out/${app}/MacOS/ES-DE"
    for theme in slate-es-de linear-es-de modern-es-de; do
      test -s "$out/share/es-de/themes/$theme/theme.xml"
    done
    test -d "$out/${app}/Resources/resources/locale"
  '';

  meta = {
    description = "ES-DE ${version} emulator frontend";
    homepage = "https://es-de.org";
    license = lib.licenses.mit;
    platforms = [ "aarch64-darwin" ];
    mainProgram = "es-de";
  };
}
