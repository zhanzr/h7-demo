# dhry_550m (app_qspi) — STM32H723ZGT6 (h723-mini) from the W25Q64 @ 0x90000000

Same sources as `bare/dhry_550m` but linked for and booted from the on-board W25Q64
at the OCTOSPI memory-mapped base `0x90000000`. Requires `h723_boot` in
internal flash.

## Results (measured from the W25Q64, 550 MHz, GCC 15.3.1, hard-float, caches on)

| Toolchain           | Flags                                          | Dhrystones/s | DMIPS/MHz |
| ------------------- | ---------------------------------------------- | ------------ | --------- |
| GCC 15.3.1          | `-Ofast -ffp-contract=fast -funroll-loops`     | 2,630,425    | 2.722     |
| armclang 6.24 (AC6) | `-Ofast -ffp-contract=fast -funroll-loops`     | **2,833,530**| **2.932** |
| ST Arm Clang 21.1.1 | `-Ofast -ffp-contract=fast -funroll-all-loops` | 2,643,171    | 2.735     |

Identical to the internal-flash build (`bare/dhry_550m`: ≤0.05% difference in
every row), so running from the W25Q64 costs nothing here. Flags come from the
same `BENCH_OPT` knob borrowed from the nano-f411 "f4-demo" benchmarks, and the
toolchain is selected with `-DSTM32_TOOLCHAIN=<gcc|armclang|starm-clang>`; **do
not add `-flto`** for Dhrystone (it inflates the score — see
`bare/dhry_550m/README.md`).

## Build & flash

```bash
bash build.sh                 # -> build/dhry_550m.hex (linked at 0x90000000)
ninja flash                   # writes the W25Q64 via the OCTOSPI algorithm
```

> **Verified on hardware**: `ninja flash` programs the W25Q64 via the OCTOSPI
> algorithm and `h723_boot` boots this app at 550 MHz. Measured from external
> flash: **2,630,425 Dhrystones/s = 2.722 DMIPS/MHz** — identical to the
> internal-flash build.

Capture the console to read the score — it is on the board's USB-serial bridge
(a CH340; `COM89` here) @ 115200:

```bash
python tools/serial_capture.py COM89 115200 20      # from the repo root
```

`tools/bench_capture.sh` cannot be used for this app: it flashes *internal*
flash, while this image lives in the W25Q64 (`ninja flash` is what programs it).
