#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="${1:-$HOME/semu}"
SCHEM="${SEMU_BIN:-$PROJECT_DIR/build/semu}"

"$SCHEM" sync status --project "$PROJECT_DIR"
"$SCHEM" sync setup --project "$PROJECT_DIR"

for _ in $(seq 1 30); do
  if curl -fsS http://127.0.0.1:8384/rest/noauth/health >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

curl -fsS http://127.0.0.1:8384/rest/noauth/health >/dev/null
"$SCHEM" sync force all --project "$PROJECT_DIR"
test -d "$PROJECT_DIR/sync/syncthing"
test -d "$PROJECT_DIR/ES-DE/ES-DE/saves/.stfolder" || true
echo "OK deck sync"
