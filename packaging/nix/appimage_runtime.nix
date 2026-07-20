{
  lib,
  stdenvNoCC,
  fetchurl,
}:

stdenvNoCC.mkDerivation {
  pname = "semu-appimage-runtime";
  version = "20251108";

  src = fetchurl {
    url = "https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64";
    hash = "sha256-L8qLRDySUQ8Ug6iD9gBhrQm0a5eLJjHIB82HOkfsJg0=";
  };

  dontUnpack = true;

  installPhase = ''
    runHook preInstall
    install -Dm0555 "$src" "$out/share/semu/appimage/runtime-x86_64"
    runHook postInstall
  '';

  meta = {
    description = "Pinned official x86_64 type-2 AppImage runtime";
    homepage = "https://github.com/AppImage/type2-runtime";
    license = lib.licenses.mit;
    platforms = [ "x86_64-linux" ];
  };
}
