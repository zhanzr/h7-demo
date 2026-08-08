#!/usr/bin/env bash
# Flash h723_boot to the H723 mini board's internal flash (ST-Link) via probe-rs.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -d /mingw64/bin ] && ! command -v cmake >/dev/null 2>&1; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
fi

cd build
ninja flash
