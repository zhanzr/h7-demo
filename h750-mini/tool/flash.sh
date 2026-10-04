#!/usr/bin/env bash
# Flash the h750-mini build to the STM32H750VB via probe-rs (Keil ULINK2/SWD).
# Cross-platform. Override: PROBE_RS=... PROBE=... CHIP=...
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HEX="$ROOT/bare/ov5640_to_st7789/build/ov5640_to_st7789.hex"

if command -v probe-rs >/dev/null 2>&1; then
    PROBE_RS="${PROBE_RS:-probe-rs}"
else
    PROBE_RS="${PROBE_RS:-$HOME/.cargo/bin/probe-rs.exe}"
fi
PROBE="${PROBE:-c251:2722:V0010M9E}"
CHIP="${CHIP:-STM32H750VB}"

"$PROBE_RS" download --probe "$PROBE" --chip "$CHIP" --protocol swd \
    --binary-format hex --verify --reset --non-interactive --disable-progressbars "$HEX"
