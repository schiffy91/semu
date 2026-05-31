{ lib, pkgs, semu }:

pkgs.runCommand "semu-desktop-entries" {} ''
  mkdir -p $out/share/applications

  cat > $out/share/applications/semu-esde.desktop <<'EOF'
[Desktop Entry]
Name=ES-DE (Semu)
Comment=Emulation frontend with all emulators bundled
Exec=${semu}/bin/es-de
Type=Application
Categories=Game;Emulator;
EOF

  cat > $out/share/applications/semu-setup.desktop <<'EOF'
[Desktop Entry]
Name=Semu Setup
Comment=Install Steam Deck defaults and generated configs
Exec=${semu}/bin/semu deck install
Type=Application
Categories=Settings;
Terminal=true
EOF
''
