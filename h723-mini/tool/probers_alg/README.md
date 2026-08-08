# probers_alg — run the OCTOSPI flash algorithm as a debuggable firmware

Runs the **exact** register-level OCTOSPI code from `../qspi_map/algo/
flash_w25q64_ospi.c` (included verbatim) as a normal firmware, starting on HSI
(64 MHz — replicating the probe-rs "connect under reset" context). It prints
the algorithm's `Init` result (JEDEC read) and the erase/program/read-back
cycle, so the OCTOSPI behaviour is visible over USART1 instead of returning
cryptic probe-rs error codes.

Used to bring up and debug the flash algorithm on hardware.

## Build & flash

```bash
bash build.sh
./flash.sh                      # probe-rs -> internal flash (ST-Link V2, SWD)
```

`ALGO_DEBUG=1` is defined for this harness only — it turns on register/SR dumps
inside the algorithm's write path (never compiled into the real algorithm by
`build_algo.py`).

## Current status

All algorithm entry points are **verified working** on hardware:
`Init` (JEDEC 0xEF4017), `0x03` reads, `EraseSector`, `ProgramPage` (16-byte
program read back as the exact pattern `50 51 52 53 54 55 56 57`).
