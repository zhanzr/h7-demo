#!/usr/bin/env bash
# Build the h750-mini cmake project (arm-none-eabi-gcc + Ninja).
# Cross-platform: works on Linux/WSL/Git Bash. Override tool paths via env:
#   CMAKE=... NINJA=...  (defaults to whatever is on PATH)
set -euo pipefail

# Prefer the MSYS2 mingw64 tools when present (native Windows binaries, so the
# build dir they configure works from any shell - see the board README
# troubleshooting note about "CreateProcess failed" during a CMake re-run).
if [ -d /mingw64/bin ]; then
    export PATH="/mingw64/bin:$PATH"
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJ="$ROOT/bare/ov5640_to_st7789"

cd "$PROJ"
cmake -G Ninja -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" -B build
cmake --build build
