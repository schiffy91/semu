{ lib, pkgs, schemulator }:

pkgs.runCommand "schemulator-desktop-entries" {} ''
  mkdir -p $out/share/applications

  cat > $out/share/applications/schemulator-esde.desktop <<'EOF'
[Desktop Entry]
Name=ES-DE (Schemulator)
Comment=Emulation frontend with all emulators bundled
Exec=${schemulator}/bin/es-de
Type=Application
Categories=Game;Emulator;
EOF

  cat > $out/share/applications/schemulator-setup.desktop <<'EOF'
[Desktop Entry]
Name=Schemulator Setup
Comment=Install Steam Deck defaults and generated configs
Exec=${schemulator}/bin/schemulator deck install
Type=Application
Categories=Settings;
Terminal=true
EOF
''
