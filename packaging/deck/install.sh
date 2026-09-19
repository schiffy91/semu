#!/bin/sh
# Semu installer for hosts without Nix (Steam Deck): verifies the tarball,
# installs it as an immutable digest-named release, keeps one previous release
# for rollback, and exposes a stable launcher.
#
#   install.sh install Semu-x86_64.tar.zst     # needs Semu-x86_64.tar.zst.sha256 beside it
#   install.sh rollback
#   install.sh status
set -eu
root="${SEMU_INSTALL_ROOT:-$HOME/Applications/Semu}"
releases="$root/releases"

digest_of() { sha256sum "$1" | cut -d' ' -f1; }

install_release() {
  artifact="$1"
  [ -f "$artifact" ] || { echo "install: missing artifact $artifact" >&2; exit 1; }
  expected="$(cut -d' ' -f1 "$artifact.sha256" 2>/dev/null || true)"
  [ -n "$expected" ] || { echo "install: missing $artifact.sha256" >&2; exit 1; }
  actual="$(digest_of "$artifact")"
  [ "$actual" = "$expected" ] || { echo "install: digest mismatch: $actual != $expected" >&2; exit 1; }
  target="$releases/$actual"
  if [ ! -d "$target" ]; then
    staging="$releases/.staging-$actual"
    rm -rf "$staging"
    mkdir -p "$staging"
    zstd -dc "$artifact" | tar -C "$staging" -xf -
    mv "$staging" "$target"
  fi
  current="$(readlink "$root/current" 2>/dev/null || true)"
  if [ -n "$current" ] && [ "$current" != "$target" ]; then
    ln -sfn "$current" "$root/previous"
  fi
  ln -sfn "$target" "$root/current"
  mkdir -p "$root/bin"
  for name in semu-deck semu-deck-cli; do
    ln -sfn "$root/current/bin/$name" "$root/bin/$name"
  done
  prune
  install_desktop_entry
  echo "installed $actual -> $root/current"
}

prune() {  # keep only current and previous
  current="$(readlink "$root/current" 2>/dev/null || true)"
  previous="$(readlink "$root/previous" 2>/dev/null || true)"
  for release in "$releases"/*; do
    [ -d "$release" ] || continue
    case "$release" in "$current"|"$previous") continue ;; esac
    rm -rf "$release"
  done
}

rollback() {
  previous="$(readlink "$root/previous" 2>/dev/null || true)"
  [ -n "$previous" ] && [ -d "$previous" ] || { echo "rollback: no previous release" >&2; exit 1; }
  current="$(readlink "$root/current")"
  ln -sfn "$previous" "$root/current"
  ln -sfn "$current" "$root/previous"
  echo "rolled back to $previous"
}

install_desktop_entry() {
  dir="$HOME/.local/share/applications"
  mkdir -p "$dir"
  cat > "$dir/semu.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Semu
Comment=Emulation frontend with every emulator bundled
Exec=$root/bin/semu-deck
Categories=Game;Emulator;
DESKTOP
}

status() {
  echo "root: $root"
  echo "current: $(readlink "$root/current" 2>/dev/null || echo none)"
  echo "previous: $(readlink "$root/previous" 2>/dev/null || echo none)"
  [ -f "$root/current/VERSION" ] && echo "version: $(cat "$root/current/VERSION")"
}

case "${1:-}" in
  install) install_release "${2:?artifact path}" ;;
  rollback) rollback ;;
  status) status ;;
  *) echo "usage: install.sh install ARTIFACT | rollback | status" >&2; exit 64 ;;
esac
