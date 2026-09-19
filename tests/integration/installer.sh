#!/bin/sh
# Contract for packaging/deck/install.sh: digest gate, digest-named releases,
# stable launcher symlinks, one previous release, rollback, pruning.
set -eu
work="${TMPDIR:-/tmp}/semu-installer"
rm -rf "$work"
mkdir -p "$work/home" "$work/src"
export HOME="$work/home" SEMU_INSTALL_ROOT="$work/root"
installer="${INSTALLER:?path to install.sh}"

release() {  # release NAME VERSION: build a tiny fake release tarball
  dir="$work/src/$1"
  mkdir -p "$dir/bin" "$dir/nix/store/x-$1"
  printf '%s\n' "$2" > "$dir/VERSION"
  printf '#!/bin/sh\necho %s\n' "$1" > "$dir/bin/semu-deck"
  printf '#!/bin/sh\necho cli-%s\n' "$1" > "$dir/bin/semu-deck-cli"
  chmod +x "$dir/bin/"*
  tar -C "$dir" -cf - . | zstd -q -o "$work/$1.tar.zst"
  (cd "$work" && sha256sum "$1.tar.zst" > "$1.tar.zst.sha256")
}

release one 1
release two 2

if sh "$installer" install "$work/one.tar.zst" >/dev/null 2>&1; then :; else echo "install one failed" >&2; exit 1; fi
[ "$("$work/root/bin/semu-deck")" = "one" ] || { echo "stable launcher does not run release one" >&2; exit 1; }
[ -f "$HOME/.local/share/applications/semu.desktop" ] || { echo "desktop entry missing" >&2; exit 1; }

cp "$work/two.tar.zst" "$work/bad.tar.zst"; cp "$work/two.tar.zst.sha256" "$work/bad.tar.zst.sha256"
printf 'x' >> "$work/bad.tar.zst"
if sh "$installer" install "$work/bad.tar.zst" >/dev/null 2>&1; then echo "corrupt artifact was accepted" >&2; exit 1; fi
[ "$("$work/root/bin/semu-deck")" = "one" ] || { echo "corrupt install changed current" >&2; exit 1; }

sh "$installer" install "$work/two.tar.zst" >/dev/null
[ "$("$work/root/bin/semu-deck")" = "two" ] || { echo "current is not release two" >&2; exit 1; }
[ "$(cat "$(readlink "$work/root/previous")/VERSION")" = "1" ] || { echo "previous is not release one" >&2; exit 1; }

sh "$installer" rollback >/dev/null
[ "$("$work/root/bin/semu-deck")" = "one" ] || { echo "rollback did not restore release one" >&2; exit 1; }
sh "$installer" rollback >/dev/null
[ "$("$work/root/bin/semu-deck")" = "two" ] || { echo "second rollback did not return to two" >&2; exit 1; }

release three 3
sh "$installer" install "$work/three.tar.zst" >/dev/null
count="$(find "$work/root/releases" -mindepth 1 -maxdepth 1 -type d | wc -l)"
[ "$count" -eq 2 ] || { echo "expected two retained releases, found $count" >&2; exit 1; }
sh "$installer" status | grep -q 'version: 3' || { echo "status does not report version 3" >&2; exit 1; }
echo "installer: pass"
