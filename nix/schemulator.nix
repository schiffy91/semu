{ lib, python3Packages, makeWrapper, symlinkJoin,
  # Emulators to bundle
  dolphin-emu, azahar,
  pcsx2 ? null, cemu ? null, retroarch-bare ? null,
  ryujinx ? null, es-de ? null,
}:

let
  setupTool = python3Packages.buildPythonApplication {
    pname = "schemulator";
    version = "0.1.0";
    src = lib.cleanSource ./..;
    format = "other";
    propagatedBuildInputs = [ python3Packages.pycryptodome ];
    installPhase = ''
      mkdir -p $out/bin $out/lib/schemulator
      cp setup.py $out/lib/schemulator/
      cp setup.json $out/lib/schemulator/
      cp decrypt3ds.py $out/lib/schemulator/
      cp -r Azahar Cemu Dolphin ES-DE Lime3DS PCSX2 RetroArch Ryujinx $out/lib/schemulator/ 2>/dev/null || true

      makeWrapper ${python3Packages.python.interpreter} $out/bin/schemulator \
        --add-flags "$out/lib/schemulator/setup.py" \
        --prefix PYTHONPATH : "${python3Packages.pycryptodome}/${python3Packages.python.sitePackages}"
    '';
    nativeBuildInputs = [ makeWrapper ];
    meta = {
      description = "Deterministic emulation environment manager";
      license = lib.licenses.mit;
    };
  };

  emulators = lib.filter (x: x != null) [
    dolphin-emu
    azahar
    pcsx2
    cemu
    retroarch-bare
    ryujinx
    es-de
  ];
in
symlinkJoin {
  name = "schemulator-full";
  paths = [ setupTool ] ++ emulators;
  meta = {
    description = "Schemulator with all emulators bundled";
  };
}
