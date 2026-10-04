#!/usr/bin/env bash
# Flash a benchmark hex via probe-rs (ST-Link V2 SWD) and capture the serial
# console (ST-Link VCP @ 115200) for a duration, then print the log.
#
# Usage:  bash tools/bench_capture.sh <hex-file> <seconds> [label]
# Env:    PROBE, CHIP, PORT, BAUD, PY  (overridable)
#
# - PROBE is optional: empty (default) lets probe-rs auto-detect the connected
#   probe. To pin one, get its selector with `probe-rs list` (e.g.
#   0483:374b:xxxx... for an ST-Link V2) and pass PROBE=...
# - PORT varies per machine - find yours (e.g. Device Manager) and pass
#   PORT=COMx, or override the default below.
#
# Example: PORT=COM3 bash tools/bench_capture.sh \
#             h723-mini/bare/dhry_550m/build/dhry_550m.hex 30 dhry-gcc
set -euo pipefail

HEX="${1:?usage: bench_capture.sh <hex-file> <seconds> [label]}"
SECS="${2:?usage: bench_capture.sh <hex-file> <seconds> [label]}"
LABEL="${3:-benchmark}"

PROBE="${PROBE:-}"
CHIP="${CHIP:-STM32H723ZG}"
PORT="${PORT:-COM3}"
BAUD="${BAUD:-115200}"
if [ -z "${PY:-}" ]; then
    for cand in python3 python; do
        if command -v "$cand" >/dev/null 2>&1 && \
           "$cand" -c "import serial" >/dev/null 2>&1; then
            PY="$cand"
            break
        fi
    done
    if [ -z "${PY:-}" ]; then
        PY="python"
    fi
fi

if ! command -v probe-rs >/dev/null 2>&1; then
    PROBE_RS="${PROBE_RS:-$HOME/.cargo/bin/probe-rs.exe}"
else
    PROBE_RS="probe-rs"
fi

PROBE_ARGS=()
if [ -n "$PROBE" ]; then
    PROBE_ARGS=(--probe "$PROBE")
fi

TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG="$(mktemp -t serial.XXXXXX).log"

echo "===== [$LABEL] flash: $(basename "$HEX") ====="
"$PROBE_RS" download "${PROBE_ARGS[@]}" --chip "$CHIP" --protocol swd \
    --binary-format hex --verify --reset --non-interactive --disable-progressbars "$HEX"

echo "===== [$LABEL] capture ${PORT} @ ${BAUD} for ${SECS}s ====="
"$PY" "$TOOLS/serial_capture.py" "$PORT" "$BAUD" "$SECS" > "$LOG"

echo "===== [$LABEL] console output ====="
cat "$LOG"
rm -f "$LOG"
