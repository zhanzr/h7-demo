# dhry_550m_qspi — STM32H723ZGT6 (h723-mini) from the W25Q64 @ 0x90000000

Same sources as `app/dhry_550m_qspi` but linked for and booted from the on-board W25Q64
at the OCTOSPI memory-mapped base `0x90000000`. Requires `h723_boot` in
internal flash.

## Build & flash

```bash
bash build.sh                 # -> build/dhry_550m_qspi.hex (linked at 0x90000000)
ninja flash                   # writes the W25Q64 via the OCTOSPI algorithm
```

> **Verified on hardware**: `ninja flash` programs the W25Q64 via the OCTOSPI
> algorithm and `h723_boot` boots this app at 550 MHz. Measured from external
> flash: **2,630,425 Dhrystones/s = 2.722 DMIPS/MHz** — identical to the
> internal-flash build.
