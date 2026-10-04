# Dhrystone 2.1 @ 480 MHz — pure-QSPI variant (dhry_480m)

Same Dhrystone 2.1 sources and flags as `dhry_480m` (12,000,000 runs,
`-Ofast -ffp-contract=fast -funroll-loops`, GCC), but the **code is linked at
`0x90000000` and executes out of the on-board W25Q64** (pure-QSPI code space),
booted by `h750_boot`. See `../QSPI_APP_GUIDE.md` for how a `_qspi` app is
built.

## Result (measured on hardware, 480 MHz, GCC 15.3.1, I/D caches on)

| Build            | Code space          | Dhrystones/s | DMIPS/MHz |
| ---------------- | ------------------- | ------------ | --------- |
| dhry_480m        | internal flash      | 2,296,651    | 2.723     |
| **dhry_480m** | **W25Q64 (QSPI)**  | **2,296,651**| **2.723** |

**Identical score.** The Dhrystone hot loop fits comfortably in the M7's
16 KB I-cache, so after the first pass the code runs from cache and the QSPI
read latency (memory-mapped 1-4-4 @ ~100 MHz) is completely hidden. There is
**no performance penalty for running Dhrystone from QSPI**.

The console also proves the boot path: `h750_boot` checks the image and jumps,
then Dhrystone prints its banner with "(from QSPI flash)".

## Build & flash

```bash
bash build.sh                 # -> build/dhry_480m.hex @ 0x90000000
# write the .hex into the W25Q64 (probe-rs W25Q64 algorithm; no --verify):
python ../qspi_map/algo/build_algo.py
probe-rs download --probe c251:2722:V0010M9E \
  --chip-description-path ../qspi_map/algo/target_w25q64.yaml \
  --chip STM32H750VB-W25Q64 --protocol swd \
  --binary-format hex --non-interactive --disable-progressbars \
  build/dhry_480m.hex
probe-rs reset --probe c251:2722:V0010M9E --chip STM32H750VB --protocol swd
```

Then capture COM56 @ 115200 (a run takes ~5.2 s).

## Notes

- The image is ~45 KB; a probe-rs algorithm write takes ~47 s (the algorithm
  now self-verifies every page - slow but self-checking).
- Must not call `Board_Init()` (it would reset the RCC and kill the QSPI clock
  the code runs from); only `HAL_Init + __enable_irq + UART_Init`.
- Flash-time measurements: internal flash openocd 3.59 s / probe-rs 19.56 s vs
  external W25Q64 bit-banged SPI ~27 s.
