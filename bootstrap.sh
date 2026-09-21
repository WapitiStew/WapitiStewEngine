#!/usr/bin/env sh
set -eu

WSE_SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$WSE_SCRIPT_DIR/bootstrap.py" "$@"
