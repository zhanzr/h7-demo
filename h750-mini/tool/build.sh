#!/usr/bin/env bash
# Build the h750-mini cmake project (arm-none-eabi-gcc + Ninja).
# Cross-platform: works on Linux/WSL/Git Bash. Override tool paths via env:
#   CMAKE=... NINJA=...  (defaults to whatever is on PATH)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJ="$ROOT/bare/ov5640_to_st7789"

cd "$PROJ"
cmake -G Ninja -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" -B build
cmake --build build
