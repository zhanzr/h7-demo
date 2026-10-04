# Dhrystone 2.1 @ 480 MHz — pure-QSPI variant (dhry_480m)

Same Dhrystone 2.1 sources and benchmark flags as `bare/dhry_480m`
(12,000,000 runs), but the **code is linked at `0x90000000` and executes out of
the on-board W25Q64** (pure-QSPI code space), booted by `h750_boot`. See
`../QSPI_APP_GUIDE.md` for how a `_qspi` app is built.

## Results (measured from the W25Q64, 480 MHz, hard-float, I/D caches on)

| Toolchain           | Flags                                          | Dhrystones/s | DMIPS/MHz |
| ------------------- | ---------------------------------------------- | ------------ | --------- |
| GCC 15.3.1          | `-Ofast -ffp-contract=fast -funroll-loops`     | 2,307,692.25 | 2.736     |
| armclang 6.24 (AC6) | `-Ofast -ffp-contract=fast -funroll-loops`     | 2,474,226.75 | 2.934     |
| ST Arm Clang 21.1.1 | `-Ofast -ffp-contract=fast -funroll-all-loops` | 2,307,248.50 | 2.736     |

All runs printed the correct final values; every toolchain works here too
(one build dir per toolchain). QSPI vs internal flash
(`../../bare/dhry_480m/README.md`):

| Configuration    | Internal flash | W25Q64 (QSPI) | Delta   |
| ---------------- | -------------- | ------------- | ------- |
| gcc default      | 2,296,650.75   | 2,307,692.25  | +0.48 % |
| armclang default | 2,474,226.75   | 2,474,226.75  | 0.0 %   |
| starm-clang      | 2,307,692.25   | 2,307,248.50  | −0.02 % |

**No measurable penalty for running Dhrystone from QSPI.** The hot loop fits
comfortably in the M7's 16 KB I-cache, so after the first pass the code runs
from cache and the memory-mapped 1-4-4 QSPI read latency (~100 MHz) is hidden;
the gcc row is even 0.48 % *higher* from QSPI (run-to-run noise) and
starm-clang 0.02 % lower.

The console also proves the boot path: `h750_boot` checks the image and jumps,
then Dhrystone prints its banner with "(from QSPI flash)".

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
force one). Console capture (CH340, `COM89` here) from `h750-mini/`:

```bash
python ../tools/serial_capture.py COM89 115200 20     # a run takes ~5.2 s
```

## Notes

- The image is ~45 KB; a probe-rs algorithm write takes ~47 s (the algorithm
  now self-verifies every page - slow but self-checking).
- Must not call `Board_Init()` (it would reset the RCC and kill the QSPI clock
  the code runs from); only `HAL_Init + __enable_irq + UART_Init`.
- Flash-time measurements: internal flash openocd 3.59 s / probe-rs 19.56 s vs
  external W25Q64 bit-banged SPI ~27 s.
