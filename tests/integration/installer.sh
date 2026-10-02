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
  mkdir -p "$dir/bin" "$dir/nix/store/x-$1"; printf %s "$1" > "$dir/nix/store/x-$1/data"
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

delta() {  # delta NAME VERSION CARRIED NEEDED...: a release tree carrying CARRIED, needing NEEDED (store path names)
  dir="$work/src/$1"; name="$1"; version="$2"; carried="$3"; shift 3
  mkdir -p "$dir/bin" "$dir/nix/store/$carried"; printf %s "$carried" > "$dir/nix/store/$carried/data"
  printf '%s\n' "$version" > "$dir/VERSION"
  printf '#!/bin/sh\necho %s\n' "$name" > "$dir/bin/semu-deck"
  printf '#!/bin/sh\necho cli-%s\n' "$name" > "$dir/bin/semu-deck-cli"
  chmod +x "$dir/bin/"*
  printf '%s\n' "$@" | sort > "$dir/store-paths"
  chmod a-w "$dir/nix/store/$carried"  # store paths arrive read-only
  tar -C "$dir" -cf - . | zstd -q -o "$work/$name.delta.tar.zst"
  (cd "$work" && sha256sum "$name.delta.tar.zst" > "$name.delta.tar.zst.sha256")
}

delta four 4 x-four x-three x-four
sh "$installer" install-delta "$work/four.delta.tar.zst" >/dev/null || { echo "install-delta four failed" >&2; exit 1; }
[ "$("$work/root/bin/semu-deck")" = "four" ] || { echo "delta four is not current" >&2; exit 1; }
[ "$(stat -c %i "$work/root/current/nix/store/x-three/data" 2>/dev/null || stat -f %i "$work/root/current/nix/store/x-three/data")" = "$(stat -c %i "$work/root/previous/nix/store/x-three/data" 2>/dev/null || stat -f %i "$work/root/previous/nix/store/x-three/data")" ] \
  || { echo "a shared store path was copied, not hard-linked from the current release" >&2; exit 1; }

cp "$work/four.delta.tar.zst" "$work/torn.delta.tar.zst"; cp "$work/four.delta.tar.zst.sha256" "$work/torn.delta.tar.zst.sha256"
printf 'x' >> "$work/torn.delta.tar.zst"
if sh "$installer" install-delta "$work/torn.delta.tar.zst" >/dev/null 2>&1; then echo "corrupt delta was accepted" >&2; exit 1; fi

delta five 5 x-five x-five x-nowhere
if sh "$installer" install-delta "$work/five.delta.tar.zst" >/dev/null 2>&1; then echo "a delta needing a path nobody has was accepted" >&2; exit 1; fi
[ "$("$work/root/bin/semu-deck")" = "four" ] || { echo "a refused delta changed current" >&2; exit 1; }

delta six 6 x-six x-four x-six
sh "$installer" install-delta "$work/six.delta.tar.zst" >/dev/null || { echo "install-delta six failed" >&2; exit 1; }
[ "$("$work/root/bin/semu-deck")" = "six" ] || { echo "delta six is not current" >&2; exit 1; }
count="$(find "$work/root/releases" -mindepth 1 -maxdepth 1 -type d | wc -l)"
[ "$count" -eq 2 ] || { echo "read-only store paths blocked pruning: $count releases kept" >&2; exit 1; }
echo "installer: pass"
