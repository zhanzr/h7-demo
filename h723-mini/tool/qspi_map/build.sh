#!/usr/bin/env bash
# Build the qspi_map two-stage firmware with CMake + Ninja.
#   bash build.sh                     - build boot (128K) + app
#   bash build.sh -DBOOT_FLASH_2M=ON  - boot linked against 2M internal flash
#   ninja flash                       - flash the bootloader to internal flash
#
# The bootloader embeds the app image (boot/src/app_image.c). app.bin is built
# first, then boot/src/app_image.c is regenerated from it, then the bootloader
# is rebuilt, so a single build.sh always ships a bootloader with the current
# app embedded.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Prefer the MSYS2 mingw64 environment (newer CMake/Ninja) when present.
if [ -d /mingw64/bin ] && ! command -v cmake >/dev/null 2>&1; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
fi

PY=""
for cand in python python3; do
    if command -v "$cand" >/dev/null 2>&1; then PY="$cand"; break; fi
done
if [ -z "${PY:-}" ]; then
    PY="/c/Users/user1/AppData/Local/Python/pythoncore-3.13-64/python.exe"
fi

mkdir -p build
cd build
cmake -G Ninja "$@" ..
ninja app_bin

# Regenerate the embedded app image from app.bin, then rebuild the bootloader.
"$PY" - "$PWD/app.bin" <<'EOF'
import sys
data = open(sys.argv[1], 'rb').read()
out = ["/* Generated from app.bin - stage-2 app image embedded in the bootloader. */",
       "#include <stdint.h>", "const uint8_t app_image[] = {"]
for i in range(0, len(data), 12):
    out.append("    " + ",".join("0x%02X" % b for b in data[i:i+12]) + ",")
out.append("};")
out.append("const uint32_t app_image_len = %d;" % len(data))
open("../boot/src/app_image.c", "w").write("\n".join(out))
print("regenerated boot/src/app_image.c (%d bytes)" % len(data))
EOF

ninja
