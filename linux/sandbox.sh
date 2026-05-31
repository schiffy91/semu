#!/usr/bin/env sh
exec "$(dirname "$0")/bin/semu-btrc" sandbox launch "$@"
