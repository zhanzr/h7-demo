# CoreMark 1.0 @ 480 MHz — pure-QSPI variant (coremark_480m)

Same CoreMark 1.0 sources and flags as `coremark_480m` (25,000 iterations,
`-Ofast -ffp-contract=fast -funroll-loops`, GCC), but the **code is linked at
`0x90000000` and executes out of the on-board W25Q64** (pure-QSPI code space),
booted by `h750_boot`. See `../QSPI_APP_GUIDE.md` for the `_qspi` rules.

## Result (measured on hardware, 480 MHz, GCC 15.3.1, I-cache on)

| Build              | Code space     | CoreMark 1.0 | Iterations/s | Total time |
| ------------------ | -------------- | ------------ | ------------ | ---------- |
| coremark_480m      | internal flash | 2070.56      | 2070.56      | 12.07 s    |
| **coremark_480m** | **W25Q64 (QSPI)** | **2064.6** | **2064.6**  | **12.12 s** |

**Runs from QSPI at essentially full speed (~0.3 % slower than internal
flash).** The ~62 KB image exceeds the 16 KB I-cache, but after warm-up the hot
benchmark loop is cache-resident, so the QSPI fetch latency (memory-mapped
1-4-4 @ 100 MHz) is almost entirely hidden - exactly the "extend the code space
reliably" behaviour expected.

## Root cause of the earlier hang (fixed)

The QSPI run used to hard-fault (`CFSR=0x00008200`, instruction-bus error +
stack error) a few seconds in. The cause was the **QUADSPI memory-mapped timeout
counter** (`CR.TCEN` + `DCR.TC`) that the driver enabled so `memmap_stop()`
could leave memory-mapped mode. With TCEN on, a gap in memory-mapped reads lets
the timeout expire mid-run, the QUADSPI stops serving `0x90000000`, and the next
code fetch bus-errors → hard fault. Dhrystone's fetch pattern never left a long
enough gap, which is why only CoreMark tripped it.

**Fix (`spi_flash_test/src/w25q64.c`):** `memmap_start()` now uses
`QSPI_TIMEOUT_COUNTER_DISABLE` (matching ST's `ExtMem_Boot` template), and
`memmap_stop()` clears BUSY explicitly with `QUADSPI_CR_ABORT` (colleague /
HAL guidance) instead of relying on the timeout.

## Build & flash

```bash
bash build.sh                 # -> build/coremark_480m.hex @ 0x90000000
# write into the W25Q64 + reset as in dhry_480m/README.md
```

## Notes

- Must not call `Board_Init()` (would reset RCC / kill the QSPI clock).
- Flash time for the 62 KB image: ~67 s (self-verifying algorithm).
- I/O compensation cell (`SYSCFG_CCCSR`) is now enabled by `w25q64_init()`
  (per ST's QSPI examples) for reliable 100 MHz QSPI drive.

## Build & flash

```bash
bash build.sh                 # -> build/coremark_480m.hex @ 0x90000000
# write into the W25Q64 + reset as in dhry_480m/README.md
```

## Notes

- Must not call `Board_Init()` (would reset RCC / kill the QSPI clock).
- The image is ~62 KB; after fixing the flash-algorithm write bug the QSPI
  CoreMark should run (slower than internal flash because the 62 KB working set
  exceeds the 16 KB I-cache, so misses go to the QSPI at ~100 MHz).
