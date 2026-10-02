#!/bin/sh
# Semu installer for hosts without Nix (Steam Deck): verifies the tarball,
# installs it as an immutable digest-named release, keeps one previous release
# for rollback, and exposes a stable launcher.
#
#   install.sh install Semu-x86_64.tar.zst     # needs Semu-x86_64.tar.zst.sha256 beside it
#   install.sh install-delta Semu-x86_64.delta.tar.zst   # a release tree plus the store paths the current one lacks
#   install.sh rollback
#   install.sh status
set -eu
root="${SEMU_INSTALL_ROOT:-$HOME/Applications/Semu}"
releases="$root/releases"

digest_of() { sha256sum "$1" | cut -d' ' -f1; }

remove_tree() { [ -e "$1" ] || [ -L "$1" ] || return 0; chmod -R u+w "$1" 2>/dev/null || true; rm -rf "$1"; }  # store paths keep their read-only modes

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
    remove_tree "$staging"
    mkdir -p "$staging"
    zstd -dc "$artifact" | tar -C "$staging" -xf -
    mv "$staging" "$target"
  fi
  switch_to "$target"
  echo "installed $actual -> $root/current"
  echo "add to Steam: quit Steam, then run $root/bin/semu-deck-cli steam shortcuts"
}

install_delta() {  # a release tree plus only the store paths the current release lacks
  artifact="$1"
  [ -f "$artifact" ] || { echo "install-delta: missing artifact $artifact" >&2; exit 1; }
  expected="$(cut -d' ' -f1 "$artifact.sha256" 2>/dev/null || true)"
  [ -n "$expected" ] || { echo "install-delta: missing $artifact.sha256" >&2; exit 1; }
  actual="$(digest_of "$artifact")"
  [ "$actual" = "$expected" ] || { echo "install-delta: digest mismatch: $actual != $expected" >&2; exit 1; }
  current="$(readlink "$root/current" 2>/dev/null || true)"
  staging="$releases/.staging-$actual"
  remove_tree "$staging"
  mkdir -p "$staging/nix/store"
  zstd -dc "$artifact" | tar -C "$staging" -xf -
  [ -f "$staging/store-paths" ] || { remove_tree "$staging"; echo "install-delta: the delta has no store-paths list" >&2; exit 1; }
  while read -r name; do  # what the delta did not carry is shared with the current release, hard-linked
    [ -e "$staging/nix/store/$name" ] && continue
    if [ -n "$current" ] && [ -e "$current/nix/store/$name" ]; then
      cp -al "$current/nix/store/$name" "$staging/nix/store/$name"
    else
      remove_tree "$staging"
      echo "install-delta: $name is neither in the delta nor in the current release" >&2
      exit 1
    fi
  done < "$staging/store-paths"
  release="$(digest_of "$staging/store-paths")"  # the release is named by what it contains
  target="$releases/$release"
  if [ -d "$target" ]; then remove_tree "$staging"; else mv "$staging" "$target"; fi
  switch_to "$target"
  echo "installed $release (delta $actual) -> $root/current"
}

switch_to() {  # make TARGET current, keep the old current as previous, refresh launchers
  target="$1"
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
}

prune() {  # keep only current and previous
  current="$(readlink "$root/current" 2>/dev/null || true)"
  previous="$(readlink "$root/previous" 2>/dev/null || true)"
  for release in "$releases"/*; do
    [ -d "$release" ] || continue
    case "$release" in "$current"|"$previous") continue ;; esac
    remove_tree "$release"
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
  install-delta) install_delta "${2:?delta path}" ;;
  rollback) rollback ;;
  status) status ;;
  *) echo "usage: install.sh install ARTIFACT | install-delta DELTA | rollback | status" >&2; exit 64 ;;
esac
