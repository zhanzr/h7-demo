# coremark_550m_qspi — STM32H723ZGT6 (h723-mini) from the W25Q64 @ 0x90000000

Same sources as `app/coremark_550m` but linked for and booted from the on-board
W25Q64 at the OCTOSPI memory-mapped base `0x90000000`. Requires `h723_boot` in
internal flash.

## Result (measured from the W25Q64, 550 MHz, GCC 15.3.1, hard-float, caches on)

| Metric         | Value                                 |
| -------------- | ------------------------------------- |
| CoreMark 1.0   | **2372.14** (25,000 iters, ~10.54 s)  |
| Validation     | `Correct operation validated.` (seedcrc 0xe9f5, crcfinal 0xcc42) |

Running entirely from external flash scores the same as the internal-flash
build (`app/coremark_550m`: 2372.59) — the 137.5 MHz OCTOSPI memory-mapped
reads keep up with the M7's I-cache.

## Build & flash

```bash
bash build.sh                 # -> build/coremark_550m_qspi.hex (linked at 0x90000000)
ninja flash                   # writes the W25Q64 via the OCTOSPI algorithm
```

> **Verified on hardware**: `ninja flash` programs the W25Q64 via the OCTOSPI
> algorithm and `h723_boot` boots this app at 550 MHz (`CoreMark 1.0 : 2372.14
> / GCC 15.3.1 ... / Static`).
