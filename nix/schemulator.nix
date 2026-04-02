{ lib, stdenv, makeWrapper, symlinkJoin, python3,
  dolphin-emu, azahar, ares ? null,
  pcsx2 ? null, cemu ? null, retroarch-bare ? null,
  ryujinx ? null, es-de ? null,
}:

let
  python = python3.withPackages (ps: [ ps.pycryptodome ]);

  setupTool = stdenv.mkDerivation {
    pname = "schemulator";
    version = "0.1.0";
    src = lib.cleanSource ./..;
    nativeBuildInputs = [ makeWrapper ];
    installPhase = ''
      mkdir -p $out/bin $out/lib/schemulator

      # Copy all project files (scripts, configs, emulator manifests)
      cp setup.py setup.json decrypt3ds.py $out/lib/schemulator/
      for dir in */; do
        if [ -f "$dir/symlinks.json" ]; then
          mkdir -p "$out/lib/schemulator/$dir"
          cp "$dir/symlinks.json" "$out/lib/schemulator/$dir/"
          # Copy portable markers if they exist
          for marker in portable.txt portable.ini; do
            [ -f "$dir/$marker" ] && cp "$dir/$marker" "$out/lib/schemulator/$dir/"
          done
        fi
      done

      makeWrapper ${python}/bin/python $out/bin/schemulator \
        --add-flags "$out/lib/schemulator/setup.py"

      makeWrapper ${python}/bin/python $out/bin/decrypt3ds \
        --add-flags "$out/lib/schemulator/decrypt3ds.py"
    '';
    meta = {
      description = "Deterministic emulation environment manager";
      license = lib.licenses.mit;
    };
  };

  emulators = lib.filter (x: x != null) [
    dolphin-emu
    azahar
    ares
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
  meta.description = "Schemulator with all emulators bundled";
}
