#!/usr/bin/env bash
# Build the STM32H750VBTx CoreMark benchmark with CMake + Ninja (Pico-style).
# Run with:  bash build.sh    (or ./build.sh on Linux)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Prefer the MSYS2 mingw64 tools when present. They are native Windows binaries,
# so the build directory they configure records a Windows cmake path and works
# from any shell. The MSYS tools would record "/usr/bin/cmake.exe", which the
# mingw64/Windows ninja cannot spawn during a CMake re-run - it fails with
# "CreateProcess failed: The system cannot find the file specified." See the
# troubleshooting note in the board README.
if [ -d /mingw64/bin ]; then
    export PATH="/mingw64/bin:$PATH"
fi
mkdir -p build
cd build
cmake -G Ninja "$@" ..
ninja
