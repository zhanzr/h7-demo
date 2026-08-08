# blink_hello_qspi — STM32H723ZGT6 (h723-mini) from the W25Q64 @ 0x90000000

Same as `app/blink_hello` (PG7 low-active LED + USART1 console), but linked for
and booted from the on-board W25Q64 at the OCTOSPI memory-mapped base
`0x90000000`. Requires `h723_boot` in internal flash.

## Build & flash

```bash
bash build.sh                 # -> build/blink_hello.hex (linked at 0x90000000)
ninja flash                   # writes the W25Q64 via the OCTOSPI algorithm
```

> **Verified on hardware**: `ninja flash` programs the W25Q64 via the OCTOSPI
> algorithm and `h723_boot` boots this app at 550 MHz (`=== blink_hello (QSPI)
> on STM32H723ZGT6 @ 550000000 Hz ===`).
