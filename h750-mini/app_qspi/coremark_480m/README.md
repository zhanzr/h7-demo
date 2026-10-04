# CoreMark 1.0 @ 480 MHz — pure-QSPI variant (coremark_480m)

Same CoreMark 1.0 sources and benchmark flags as `bare/coremark_480m`
(25,000 iterations), but the **code is linked at `0x90000000` and executes out
of the on-board W25Q64** (pure-QSPI code space), booted by `h750_boot`. See
`../QSPI_APP_GUIDE.md` for the `_qspi` rules.

## Results (measured from the W25Q64, 480 MHz, hard-float, I/D caches on)

| Toolchain           | Flags                                          | CoreMark 1.0 | Total time |
| ------------------- | ---------------------------------------------- | ------------ | ---------- |
| GCC 15.3.1          | `-Ofast -ffp-contract=fast -funroll-all-loops` | 2059.14      | 12.141 s   |
| GCC 15.3.1          | + `-DSTM32_LTO=ON`                             | 1899.84      | 13.159 s   |
| armclang 6.24 (AC6) | `-Ofast -ffp-contract=fast -funroll-loops`     | 2090.48      | 11.959 s   |
| armclang 6.24 (AC6) | `-Omax -fno-lto` (via `BENCH_OPT_C`)           | **2384.81**  | 10.483 s   |
| ST Arm Clang 21.1.1 | `-Ofast -ffp-contract=fast -funroll-all-loops` | 1801.80      | 13.875 s   |

All five rows print **`Correct operation validated.`** and report the flags
actually used (`Compiler flags : ...`): `FLAGS_STR` is generated from
`BENCH_OPT` + `BENCH_OPT_C`, so the console line cannot drift from the build.

QSPI vs internal flash, matching row by row (internal-flash twin:
`../../bare/coremark_480m/README.md`):

| Configuration             | Internal | QSPI    | Delta    |
| ------------------------- | -------- | ------- | -------- |
| gcc default               | 2127.84  | 2059.14 | −3.2 %   |
| gcc + LTO                 | 1996.01  | 1899.84 | −4.8 %   |
| armclang default          | 2090.48  | 2090.48 | 0.0 %    |
| armclang `-Omax -fno-lto` | 2494.51  | 2384.81 | −4.4 %   |
| starm-clang               | 1801.93  | 1801.80 | −0.007 % |

The ~62 KB image exceeds the 16 KB I-cache, but after warm-up the hot benchmark
loop is cache-resident, so the memory-mapped 1-4-4 QSPI fetch latency @ 100 MHz
costs almost nothing: armclang's default build and starm-clang match internal
flash to <0.01 %. The flags and the `BENCH_OPT` / `BENCH_OPT_C` / `STM32_LTO` /
`STM32_TOOLCHAIN` knobs are the bare-metal twin's (borrowed from the nano-f411
"f4-demo" benchmarks); armclang `-Omax -fno-lto` is fastest here too and LTO
stays **off** — see `../../bare/coremark_480m/README.md` for the full
discussion.

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
# GNU arm-none-eabi-gcc (default); one build dir per toolchain
mkdir -p build && cd build
cmake -G Ninja -DSTM32_TOOLCHAIN=gcc .. && ninja
ninja flash                   # QUADSPI algorithm writes the W25Q64

# ST Arm Clang instead (armclang works the same way):
mkdir -p ../build-starm && cd ../build-starm
cmake -G Ninja -DSTM32_TOOLCHAIN=starm-clang .. && ninja && ninja flash
```

`ninja flash` needs `h750-mini/tool/h750_boot` in internal flash first — the
bootloader validates the image at `0x90000000` and jumps to it. probe-rs
auto-detects the attached probe (`flash-stlink` / `flash-dap` / `flash-jlink`
force one).

Capture the console (CH340, `COM89` here) from `h750-mini/`:

```bash
python ../tools/serial_capture.py COM89 115200 20
```

## Notes

- Must not call `Board_Init()` (would reset RCC / kill the QSPI clock).
- The `.hex` is written to the W25Q64 by `ninja flash` through the
  self-verifying hardware-QUADSPI algorithm (`target_w25q64_qspi.yaml`,
  page read-back verification).
- I/O compensation cell (`SYSCFG_CCCSR`) is now enabled by `w25q64_init()`
  (per ST's QSPI examples) for reliable 100 MHz QSPI drive.
