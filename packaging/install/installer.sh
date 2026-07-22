#!/usr/bin/env sh
set -eu
umask 077

case "${1:-}" in
  install|status|rollback) ;;
  *) set -- install "$@" ;;
esac
action="$1"
install_root="${SEMU_INSTALL_ROOT:-$HOME/Applications/Semu}"
artifact="${SEMU_ARTIFACT:-}"
expected_sha256="${SEMU_SHA256:-}"
pending=""
parsing_options=1
for argument in "$@"; do
  if [ "$pending" = install_root ]; then install_root="$argument"; pending=""; continue; fi
  if [ "$pending" = artifact ]; then artifact="$argument"; pending=""; continue; fi
  if [ "$pending" = sha256 ]; then expected_sha256="$argument"; pending=""; continue; fi
  [ "$parsing_options" = 1 ] || continue
  case "$argument" in
    --) parsing_options=0 ;;
    --install-root) pending=install_root ;;
    --install-root=*) install_root="${argument#*=}" ;;
    --artifact) pending=artifact ;;
    --artifact=*) artifact="${argument#*=}" ;;
    --sha256) pending=sha256 ;;
    --sha256=*) expected_sha256="${argument#*=}" ;;
  esac
done
[ -z "$pending" ] || {
  echo "Semu installer: --$pending requires a value." >&2
  exit 2
}

runtime="${SEMU_INSTALL_CLI:-$install_root/libexec/semu-btrc}"
[ "$action" = install ] || {
  if [ -x "$runtime" ]; then exec "$runtime" install "$@"; fi
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
expected_sha256="$(printf '%s' "$expected_sha256" | tr '[:upper:]' '[:lower:]')"
if [ "${#expected_sha256}" -ne 64 ]; then
  echo "Semu installer: install requires --sha256 with 64 hexadecimal characters." >&2
  exit 2
fi
case "$expected_sha256" in
  *[!0-9a-f]*)
    echo "Semu installer: --sha256 must be a hexadecimal SHA-256 digest." >&2
    exit 2
    ;;
esac
if [ -x "$runtime" ]; then
  exec "$runtime" install "$@" --sha256 "$expected_sha256"
fi

temporary="$(mktemp -d "${TMPDIR:-/tmp}/semu-install.XXXXXX")"
trap 'rm -rf -- "$temporary"' EXIT HUP INT TERM
snapshot="$temporary/Semu.AppImage"
extraction="$temporary/extract"
mkdir -m 0700 -- "$extraction"
cp -- "$artifact" "$snapshot"
chmod 0500 "$snapshot"
actual_sha256="$(sha256sum -- "$snapshot")"
actual_sha256="${actual_sha256%% *}"
[ "$actual_sha256" = "$expected_sha256" ] || {
  echo "Semu installer: AppImage sha256 does not match the requested bytes." >&2
  exit 2
}
(
  unset APPDIR APPIMAGE APPIMAGE_EXTRACT_AND_RUN NO_CLEANUP
  cd "$extraction"
  "$snapshot" --appimage-extract >/dev/null
)
bootstrap="$extraction/squashfs-root"
runtime="$bootstrap/lib/semu/semu-btrc"
source_root="$bootstrap/share/semu/config"
[ -x "$runtime" ] && [ -d "$source_root" ] || {
  echo "Semu installer: extracted AppImage has no static install command." >&2
  exit 2
}
trap - EXIT HUP INT TERM
exec "$runtime" install "$@" --artifact "$snapshot" \
  --sha256 "$expected_sha256" --source-root "$source_root" \
  --bootstrap-root "$bootstrap" --bootstrap-temporary "$temporary"
