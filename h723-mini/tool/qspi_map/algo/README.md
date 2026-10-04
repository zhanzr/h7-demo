# OCTOSPI W25Q64 flash algorithm (`algo/`)

Position-independent flash algorithm for programming the on-board W25Q64 (8 MB)
via probe-rs, using the STM32H723ZG OCTOSPI1 in 1-line SPI mode (no QE needed).

## Files

- `flash_w25q64_ospi.c` — the algorithm (register-level OCTOSPI, no HAL).
- `algo.ld` — links it at `0x20000000` for the position-independent blob.
- `build_algo.py` — compiles it and generates `target_w25q64_ospi.yaml`
  (`python build_algo.py flash_w25q64_ospi.c 0x4000`).
- `target_w25q64_ospi.yaml` — probe-rs chip description (auto-generated).

## Status (verified on hardware)

**Fully working** (fixed during bring-up with `../probers_alg`). The full flow is
proven on the board: `ninja flash` on any `app_qspi/<app>` programs the W25Q64 and
`h723_boot` boots it at 550 MHz. Verified apps: `app_qspi/blink_hello` and
`app_qspi/dhry_550m` (2.722 DMIPS/MHz from external flash — matches internal flash).

| Function     | Status |
|--------------|--------|
| `Init` (JEDEC) | **OK** (0xEF4017) |
| `0x03` read   | **OK** |
| `EraseSector` | **OK** |
| `ProgramPage` | **OK** (multi-page + 256-byte W25Q64 page-boundary split) |
| `Verify`      | **OK** |

## Root causes fixed (the write path)

Debugged with `../probers_alg` on the real board. The write path failed because
the register sequence did not match what `HAL_OSPI_Transmit`/`HAL_OSPI_Command`
actually do. The fixes:

1. **FMODE encoding**: `CR[29:28]` FMODE `00` = indirect **write**, `01` =
   indirect read, `10` = auto-polling, `11` = memory-mapped (HAL
   `OSPI_FUNCTIONAL_MODE_*`). A data write must clear FMODE to `0`; setting it
   to `0x2<<28` puts the OSPI in auto-polling mode and the TX data never goes out.
2. **Command-only transfers (n == 0) auto-start** once configured — do NOT
   re-write IR/AR to trigger. The old trigger re-write left the OSPI stuck BUSY.
3. **Wait for TCF** (transfer complete) after reads/writes, exactly like the
   HAL, instead of polling BUSY.
4. Config write order matches `OSPI_ConfigCmd`: clear FMODE first, then CCR,
   TCR, DLR, IR, AR.

Other context notes from bring-up:

- The clock **prescaler lives in DCR2** (not CR).
- The **sample-shift must be set in TCR.SSHIFT** (0x40000000) for reliable reads.
- The **OSPIM (I/O manager) PCR0 = 0x02010101** + CR REQ2ACK_TIME = 0xFF route
  the OSPI signals to port 1 (GPIOF/G).
- **PG6 (NCS)** stays AF10 (hardware CS).
- Reads must **drain the residual RX FIFO** (shared FIFO) before the next
  transfer.
