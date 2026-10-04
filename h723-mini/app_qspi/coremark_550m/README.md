# coremark_550m (app_qspi) — STM32H723ZGT6 (h723-mini) from the W25Q64 @ 0x90000000

Same sources as `bare/coremark_550m` but linked for and booted from the on-board
W25Q64 at the OCTOSPI memory-mapped base `0x90000000`. Requires `h723_boot` in
internal flash.

## Results (measured from the W25Q64, 550 MHz, hard-float, caches on)

| Toolchain           | Flags                                          | CoreMark 1.0 | Total time |
| ------------------- | ---------------------------------------------- | ------------ | ---------- |
| GCC 15.3.1          | `-Ofast -ffp-contract=fast -funroll-all-loops` | 2440.21      | 10.245 s   |
| GCC 15.3.1          | + `-DSTM32_LTO=ON`                             | 2287.70      | 10.928 s   |
| armclang 6.24 (AC6) | `-Ofast -ffp-contract=fast -funroll-loops`     | 2394.64      | 10.440 s   |
| armclang 6.24 (AC6) | `-Omax -fno-lto` (via `BENCH_OPT_C`)           | **2864.67**  | 8.727 s    |
| ST Arm Clang 21.1.1 | `-Ofast -ffp-contract=fast -funroll-all-loops` | 2063.90      | 12.113 s   |

All rows validate (`Correct operation validated.`, seedcrc 0xe9f5, crcfinal
0xcc42) and print the flags actually used.

Running entirely from external flash scores the same as the internal-flash build
(0.02–0.04% slower than the matching `bare/coremark_550m` row in every case) —
the 137.5 MHz OCTOSPI memory-mapped reads keep up with the M7's I-cache. The
flags and the `BENCH_OPT` / `BENCH_OPT_C` / `STM32_LTO` / `STM32_TOOLCHAIN` knobs
are the same as the bare-metal twin's (borrowed from the nano-f411 "f4-demo"
benchmarks): **armclang `-Omax -fno-lto` is fastest and LTO costs 6.2% on this
board** — see `bare/coremark_550m/README.md` for the full discussion.

## Build & flash

```bash
bash build.sh                 # -> build/coremark_550m.hex (linked at 0x90000000)
ninja flash                   # writes the W25Q64 via the OCTOSPI algorithm
```

Every toolchain in `bare/coremark_550m/README.md` works here too: swap `build/`
for `build-ac6/` / `build-starm/`, pass the same `-DSTM32_TOOLCHAIN=` /
`-DBENCH_OPT*` flags, then run `ninja flash` from that build dir.

> **Verified on hardware**: `ninja flash` programs the W25Q64 via the OCTOSPI
> algorithm and `h723_boot` boots this app at 550 MHz (`CoreMark 1.0 : 2440.21
> / GCC 15.3.1 ... / Static`).

Capture the console to read the score — it is on the board's USB-serial bridge
(a CH340; `COM89` here) @ 115200:

```bash
python tools/serial_capture.py COM89 115200 30      # from the repo root
```

`tools/bench_capture.sh` cannot be used for this app: it flashes *internal*
flash, while this image lives in the W25Q64 (`ninja flash` is what programs it).
