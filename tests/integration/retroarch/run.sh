#!/usr/bin/env bash
set -euo pipefail

here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
repository="$(CDPATH= cd -- "$here/../../.." && pwd -P)"
results="$(mktemp -d "${TMPDIR:-/tmp}/semu-retroarch-real.XXXXXX")"
image="localhost/semu-retroarch-real-integration:nix-2.31.2"
store_volume="semu-retroarch-real-nix-store"
nix_config="$(printf '%s\n' 'experimental-features = nix-command flakes' 'sandbox = false' 'filter-syscalls = false' 'max-jobs = auto' 'cores = 0')"

cleanup() {
  local status=$?
  if [[ "$status" -eq 0 ]]; then
    chmod -R u+w "$results" 2>/dev/null || true
    rm -rf "$results"
  else
    printf 'retroarch integration artifacts: %s\n' "$results" >&2
  fi
}
trap cleanup EXIT

podman build --platform linux/amd64 --quiet --tag "$image" --file "$here/Containerfile" "$here"
podman volume inspect "$store_volume" >/dev/null 2>&1 || podman volume create "$store_volume" >/dev/null

podman run --rm --platform linux/amd64 --env NIX_CONFIG="$nix_config" --volume "$store_volume:/nix" --volume "$repository:/repository:ro" --volume "$results:/results" "$image" sh -euc '
    runtime="$(nix build --impure --file /repository/tests/integration/retroarch/runtime.nix --no-link --print-out-paths)"
    /repository/tests/integration/retroarch/scenario.sh "$runtime" /results
  '

printf '%s\n' "retroarch_real_integration=pass runtime=source-patched display=xorg-dummy renderer=llvmpipe"
