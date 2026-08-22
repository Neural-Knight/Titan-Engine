#!/usr/bin/env bash
# Runs $1 under `perf stat -d` on Linux; direct on any other platform.
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <benchmark-binary> [args...]" >&2
    exit 1
fi

BIN="$1"
shift

if [[ "$(uname)" == "Linux" ]]; then
    exec perf stat -d "$BIN" "$@"
else
    echo "perf unavailable on $(uname) -- running $BIN directly" >&2
    exec "$BIN" "$@"
fi
