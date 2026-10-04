# qspi_map — two-stage boot from the W25Q64 (OCTOSPI memory-mapped)

Demonstrates running a stage-2 app entirely from the on-board W25Q64 at the
OCTOSPI memory-mapped base `0x90000000`:

- `boot/` — stage-1 bootloader in internal flash (owns the 550 MHz clock tree,
  initializes the OCTOSPI + W25Q64, checks + jumps to `0x90000000`).
- `app/` (inside `tool/qspi_map/`) — stage-2 app linked at `0x90000000` (blink_hello-like; uses the
  non-destructive `system_app.c`).
- `algo/` — the probe-rs OCTOSPI flash algorithm that programs the W25Q64.

## Build

```bash
bash build.sh              # -> build/boot.elf/.hex + build/app.elf/.hex/.bin
ninja                      # build both
ninja flash_boot           # probe-rs -> boot.hex into internal flash (auto-detect)
ninja flash_boot-stlink    # ... or force a probe family (-dap / -jlink / -ulink)
ninja bin                  # app.raw .bin (for ext-flash download)
```

## Flashing the stage-2 app into the W25Q64

The stage-2 app is written to the W25Q64 with:

1. **probe-rs OCTOSPI algorithm** (`algo/`) — `cd ../app_qspi/<app> && ninja
   flash`. ✅ **Verified working on hardware**: `Init` (JEDEC read), `0x03`
   reads, `EraseSector`, `ProgramPage` and `Verify` all pass, and `h723_boot`
   boots `app_qspi/blink_hello` / `app_qspi/dhry_550m` from the W25Q64 at 550 MHz.
2. **openocd `stmsmi`** (for OCTOSPI-class parts, driver compiled into the
   installed openocd 0.12.0): write a board TCL that enables the OSPI
   clocks + GPIO and creates a flash bank at `0x90000000`, then
   `openocd ... -c "program app.hex verify reset exit"`.

## Notes

- The MPU marks `0x90000000..0x9FFFFFFF` cacheable + executable (needed to
  run code from the OSPI).
- The stage-2 app must NOT reconfigure RCC/OSPI clocks (the bootloader owns
  them) — see `app/src/system_app.c`.
