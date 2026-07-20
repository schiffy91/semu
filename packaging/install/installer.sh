#!/usr/bin/env sh
set -eu

case "${1:-}" in
  install|status|rollback) ;;
  *) set -- install "$@" ;;
esac
action="$1"
install_root="${SEMU_INSTALL_ROOT:-$HOME/Applications/Semu}"
artifact="${SEMU_ARTIFACT:-}"
pending=""
for argument in "$@"; do
  if [ "$pending" = install_root ]; then install_root="$argument"; pending=""; continue; fi
  if [ "$pending" = artifact ]; then artifact="$argument"; pending=""; continue; fi
  case "$argument" in
    --install-root) pending=install_root ;;
    --install-root=*) install_root="${argument#*=}" ;;
    --artifact) pending=artifact ;;
    --artifact=*) artifact="${argument#*=}" ;;
  esac
done

runtime="${SEMU_INSTALL_CLI:-$install_root/libexec/semu-btrc}"
if [ -x "$runtime" ]; then exec "$runtime" install "$@"; fi
[ "$action" = install ] || {
  echo "Semu installer: no installed static runtime handles '$action'." >&2
  exit 2
}
[ "$(uname -s)" = Linux ] || {
  echo "Semu installer: first installation requires the Linux AppImage target." >&2
  exit 2
}
[ -n "$artifact" ] && [ -f "$artifact" ] && [ ! -L "$artifact" ] || {
  echo "Semu installer: --artifact must name a regular AppImage." >&2
  exit 2
}

temporary="$(mktemp -d "${TMPDIR:-/tmp}/semu-install.XXXXXX")"
trap 'rm -rf -- "$temporary"' EXIT HUP INT TERM
cp -- "$artifact" "$temporary/Semu.AppImage"
chmod 0500 "$temporary/Semu.AppImage"
(
  unset APPDIR APPIMAGE APPIMAGE_EXTRACT_AND_RUN NO_CLEANUP
  cd "$temporary"
  ./Semu.AppImage --appimage-extract >/dev/null
)
bootstrap="$temporary/squashfs-root"
runtime="$bootstrap/lib/semu/semu-btrc"
source_root="$bootstrap/share/semu/config"
[ -x "$runtime" ] && [ -d "$source_root" ] || {
  echo "Semu installer: extracted AppImage has no static install command." >&2
  exit 2
}
trap - EXIT HUP INT TERM
exec "$runtime" install "$@" --source-root "$source_root" \
  --bootstrap-root "$bootstrap" --bootstrap-temporary "$temporary"
