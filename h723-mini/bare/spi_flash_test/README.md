# spi_flash_test

W25Q64 (8 Mbyte) **OCTOSPI** flash benchmark + XIP demo for the h723-mini board
(STM32H723ZGT6 @ 550 MHz, W25Q64 on OCTOSPI1 port 1).

The driver uses the OCTOSPI HAL (this board's W25Q64 hangs on OCTOSPI1, not a
QUADSPI). Pins (vendor `6.OSPI` example): PF8 IO0 / PF9 IO1 / PF7 IO2 /
PF6 IO3 / PF10 CLK / PG6 NCS. OSPI kernel clock = D1HCLK (275 MHz) with
ClockPrescaler 2 → **137.5 MHz**.

Measures, printed over USART1 @ 115200 (`COMxx`), re-run every second:

- **erase** (line-mode independent, always 1-1-1): 4K sector / 32K block / 64K block
- **write** (page program): 1-1-1 (0x02) vs 1-1-4 (0x32)  (W25Q64 has **no**
  2-line page-program command, so only 1 and 4 data-line writes exist)
- **read, indirect HAL** (FIFO polling): 1-1-1 / 1-1-2 / 1-2-2 / 1-1-4 / 1-4-4
- **read, memory-mapped** (0x90000000 + offset): same 5 line modes
- **XIP**: copies a 16-byte position-independent function into flash sector 0
  and executes it from the mapped space

Every read/write is checksum-verified against the source pattern, so a
`FAIL` means genuinely corrupt data, not just a slow link.

## Measured results (this board, 137.5 MHz OSPI clock)

| operation              | result                             |
|------------------------|------------------------------------|
| erase 4K  sector       | ~40 ms                             |
| erase 32K block        | ~116 ms                            |
| erase 64K block        | ~154 ms                            |
| write 64K, 1-1-1       | ~97 ms  (~656 KiB/s)               |
| write 64K, 1-1-4       | ~97 ms  (~660 KiB/s)               |
| read 256K, HAL poll    | ~11.9 MiB/s in **all** 5 modes     |
| read 256K, memmap 1-1-1| 16.39 MiB/s                        |
| read 256K, memmap 1-1-2| 32.78 MiB/s                        |
| read 256K, memmap 1-2-2| 32.78 MiB/s                        |
| read 256K, memmap 1-1-4| 65.56 MiB/s                        |
| read 256K, memmap 1-4-4| 65.56 MiB/s                        |
| XIP execute            | OK (correct result)                |

Notes:

- **Write speed is identical for 1-1-1 and 1-1-4** — page program is dominated
  by the flash's internal program time (~0.4 ms/page), not the wire transfer.
- **HAL polling reads are CPU-bound** (`HAL_OSPI_Receive` polls one flag per
  byte), so all 5 modes measure ~11.9 MiB/s. Use memory-mapped mode for real
  throughput.
- **Memory-mapped reads scale exactly with line count** (16.4 / 32.8 / 65.6
  MiB/s for 1x / 2x / 4x), with the D-cache enabled (`mpu_ospi_config`), up to
  the 65.6 MiB/s ceiling of the 137.5 MHz OSPI clock at 4 data lines.

## Build + flash

```bash
bash build.sh                 # CMake + Ninja, GNU arm-none-eabi-gcc
ninja                         # builds spi_flash_test.elf + .hex
ninja flash                   # probe-rs, STM32H723ZG (image is < 1 MB)
```

## Driver design (`src/w25q64.c/.h`)

- **OSPI clock = 137.5 MHz** (D1HCLK 275 MHz, prescaler 2 — the vendor
  `6.OSPI` default; the W25Q64JV is rated 133 MHz, but this config is stable
  on the vendor board).
- **Quad Enable (QE) is set during init** (WP# trick: drive IO2/PF7 high as a
  GPIO while writing SR2, try `0x31` then fall back to `0x01`). Idempotent:
  skips if SR2.QE is already 1.
- **Continuous-read mode is exited explicitly** via `0xF0` alternate bytes for
  the 1-2-2 / 1-4-4 I/O reads.
- **Memory-mapped exit uses `HAL_OSPI_Abort`**: leaving FMODE = memory-mapped
  is done by the next `HAL_OSPI_Command` (its `OSPI_ConfigCmd` re-initializes
  `OCTOSPI_CR_FMODE` to indirect-write). A manual abort that issued a status
  command without a matching `HAL_OSPI_Receive` left the handle stuck in
  `HAL_OSPI_STATE_CMD_CFG`, breaking every later indirect command — fixed by
  using `HAL_OSPI_Abort` alone.
- `stm32h7xx_hal_mdma.c` is linked because `HAL_OSPI_Abort` references
  `HAL_MDMA_Abort`.

## vs. the vendor driver (`ospi_w25q64.c`, `6.OSPI`)

The vendor example never sets SR2.QE (its 0xEB/0x32 quad commands only work if
the flash was pre-programmed with QE=1), has no memory-mapped exit path, and
returns bare error codes. This driver fixes those.

## XIP / code-in-mapped-space

OCTOSPI1 memory-mapped base = **0x90000000**. The firmware writes the
position-independent function from `src/qspi_xip.S` (`.qspi_code` section,
exported via `__qspi_code_start/__qspi_code_end` in `spi_flash_test.ld`) into
flash sector 0 and calls it at `0x90000000 + offset`. MPU region 0 marks
0x90000000..0x9FFFFFFF as normal, cacheable, **executable** (see
`mpu_ospi_config()` in `main.c`).
