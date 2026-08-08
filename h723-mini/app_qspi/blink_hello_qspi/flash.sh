#!/usr/bin/env bash
# Flash blink_hello_qspi into the on-board W25Q64 (0x90000000) via the
# probe-rs OCTOSPI flash algorithm (ST-Link V2, SWD). h723_boot must already
# be in internal flash.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -d /mingw64/bin ] && ! command -v cmake >/dev/null 2>&1; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
fi

cd build
ninja flash
