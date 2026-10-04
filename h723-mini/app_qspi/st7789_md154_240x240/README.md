# st7789_md154_240x240 (app_qspi) — STM32H723ZGT6 (h723-mini) from the W25Q64 @ 0x90000000

Same sources as `bare/st7789_md154_240x240` but linked for and booted from the on-board W25Q64
at the OCTOSPI memory-mapped base `0x90000000`. Requires `h723_boot` in
internal flash.

## Build & flash

```bash
bash build.sh                 # -> build/st7789.hex (linked at 0x90000000)
ninja flash                   # writes the W25Q64 via the OCTOSPI algorithm
```

> **Verified on hardware**: `ninja flash` programs the W25Q64 via the OCTOSPI
> algorithm and `h723_boot` boots this app at 550 MHz (the `app_qspi/blink_hello`
> and `app_qspi/dhry_550m` twins use the same flash + boot flow).
