#!/usr/bin/env bash
# Reject unreviewed media and generated payloads in the index or supplied history.
set -euo pipefail
root=$(git rev-parse --show-toplevel)
cd "$root"
approved=config/input/steam/icons/APPROVED.txt
objects=$(mktemp)
trap 'rm -f "$objects"' EXIT
if [ "$#" -eq 0 ]; then
  git ls-files --stage | awk '{ oid=$2; sub(/^[^\t]*\t/, ""); print oid " " $0 }' >"$objects"
else
  git rev-list --objects "$@" >"$objects"
fi
awk '
  NR == FNR { if ($1 !~ /^#/ && NF >= 2) approved[$1] = 1; next }
  {
    oid=$1; sub(/^[^ ]+ ?/, ""); path=tolower($0)
    media = path ~ /\.(png|jpe?g|webp|gif|bmp|svg|mp4|mkv|webm|wav|mp3|ogg|flac|ttf|otf|gz|zst|zip|7z|rar|so|dylib|dll|exe|bin|rom|iso|chd|nes|sfc|smc|gb|gbc|gba|nds|3ds|cia|n64|v64|z64|nsp|xci|wbfs|wud|wux|wad|keys)$/
    generated = path ~ /(^|\/)(deck-shots|zoom|garbage|generated)(\/|$)/
    keys = path ~ /(^|\/)(keys.txt|prod.keys|title.keys)$/
    if ((media || generated || keys) && !(oid in approved)) {
      print "Unreviewed repository payload: " $0 " (" oid ")" > "/dev/stderr"
      failed=1
    }
  }
  END { exit failed }
' "$approved" "$objects"
printf '%s\n' 'Repository content check passed.'
