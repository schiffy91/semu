# Steam Deck / desktop integration
# Creates .desktop files for ES-DE and individual emulators
# so they appear in the application menu and can be added as non-Steam games.
{ lib, pkgs, schemulator, ... }:

pkgs.runCommand "schemulator-desktop-entries" {} ''
  mkdir -p $out/share/applications

  cat > $out/share/applications/schemulator-esde.desktop << 'EOF'
  [Desktop Entry]
  Name=ES-DE (Schemulator)
  Comment=Emulation frontend with all emulators bundled
  Exec=${schemulator}/bin/es-de
  Type=Application
  Categories=Game;Emulator;
  Icon=es-de
  EOF

  cat > $out/share/applications/schemulator-setup.desktop << 'EOF'
  [Desktop Entry]
  Name=Schemulator Setup
  Comment=Set up emulator symlinks and configs
  Exec=${schemulator}/bin/schemulator symlink
  Type=Application
  Categories=Settings;
  Terminal=true
  EOF
''
