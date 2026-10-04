#!/usr/bin/env bash
# Build the STM32H750 (h750-mini) HSE test with CMake + Ninja.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Prefer the MSYS2 mingw64 tools when present (native Windows binaries, so the
# build dir works from any shell - see the board README troubleshooting note).
if [ -d /mingw64/bin ]; then
    export PATH="/mingw64/bin:$PATH"
fi
mkdir -p build
cd build
cmake -G Ninja "$@" ..
ninja
