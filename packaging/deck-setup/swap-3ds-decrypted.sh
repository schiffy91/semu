#!/usr/bin/env bash
# Replace the Steam Deck's ENCRYPTED 3DS ROMs with the DECRYPTED ones from the Drive.
# Azahar runs decrypted .3ds with no AES keys. Safe: no --delete (SD-only games kept);
# --checksum (only differing files copied, idempotent/resumable); the Drive copy is the backup.
set -u
SRC="$HOME/Drive/Media/Games/Emulation/ES-DE/ES-DE/ROMs/n3ds/"
DST="deck@steamdeck.local:/run/media/deck/SD/Emulation/ES-DE/ES-DE/ROMs/n3ds/"
echo "Swapping decrypted 3DS ROMs $SRC -> $DST"
rsync -rt --checksum --partial-dir=.rsync-partial --info=progress2 \
  --include='*.3ds' --exclude='*' \
  -e "ssh -o BatchMode=yes -o ConnectTimeout=20 -o ServerAliveInterval=15" \
  "$SRC" "$DST"
echo "rsync exit=$?"
