#!/usr/bin/env bash
# Build the STM32H750VBTx ov5640_to_st7789 QSPI camera/display demo (code runs
# from the W25Q64 at 0x90000000) with CMake + Ninja.
# Run with:  bash build.sh    (or ./build.sh on Linux)
# This project reuses the CubeMX sources from ../../cubemx_file (Core + Drivers).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Prefer the MSYS2 mingw64 environment (newer CMake/Ninja) when present.
if [ -d /mingw64/bin ] && ! command -v cmake >/dev/null 2>&1; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
fi

mkdir -p build
cd build
cmake -G Ninja "$@" ..
ninja
