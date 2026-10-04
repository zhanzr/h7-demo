# coremark_550m (app_qspi) — STM32H723ZGT6 (h723-mini) from the W25Q64 @ 0x90000000

Same sources as `bare/coremark_550m` but linked for and booted from the on-board
W25Q64 at the OCTOSPI memory-mapped base `0x90000000`. Requires `h723_boot` in
internal flash.

## Results (measured from the W25Q64, 550 MHz, GCC 15.3.1, hard-float, caches on)

| Flags                                          | CoreMark 1.0 | Total time |
| ---------------------------------------------- | ------------ | ---------- |
| `-Ofast -ffp-contract=fast -funroll-all-loops` | **2440.21**  | 10.245 s   |
| + `-DSTM32_LTO=ON`                             | 2287.70      | 10.928 s   |

Both configurations validate (`Correct operation validated.`, seedcrc 0xe9f5,
crcfinal 0xcc42).

Running entirely from external flash scores the same as the internal-flash build
(`bare/coremark_550m`: 2440.93 vs 2440.21 — 0.03% slower) — the 137.5 MHz OCTOSPI
memory-mapped reads keep up with the M7's I-cache. The flags and the `BENCH_OPT`
/ `BENCH_OPT_C` / `STM32_LTO` knobs are borrowed from the nano-f411 "f4-demo"
benchmarks; as on the bare-metal twin, **LTO costs 6.2% here** (2287.70 vs
2440.21) and stays off by default.

## Build & flash

```bash
bash build.sh                 # -> build/coremark_550m.hex (linked at 0x90000000)
ninja flash                   # writes the W25Q64 via the OCTOSPI algorithm
```

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
