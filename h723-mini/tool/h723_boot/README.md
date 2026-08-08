# h723_boot — minimal STM32H723ZG bootloader (internal flash)

A small bootloader for the h723-mini (STM32H723ZGT6) that lives in the internal
flash. On every reset it:

1. brings up the 550 MHz clock tree + USART1 console + LED (PG7),
2. initializes the W25Q64 (8 MB) via the OCTOSPI,
3. performs a **basic** bootability check on a stage-2 firmware at the OSPI
   memory-mapped base `0x90000000`:
   - word 0 (initial stack top) must land in DTCM/SRAM `0x20000000..0x24000000`,
   - word 1 (reset vector) must be a thumb pointer inside `0x90000000..0x90800000`,
4. if the check **passes**: enters OCTOSPI memory-mapped mode and jumps,
5. if the check **fails**: loops forever, toggling the PG7 LED and printing the
   check result once per second.

## Build & flash

```bash
bash build.sh                        # cmake -G Ninja + ninja -> build/h723_boot.hex
cd build && ninja flash              # probe-rs -> internal flash (ST-Link V2, SWD)
```

## Console output

PASS path:

```
=== h723_boot bootloader @ 550000000 Hz ===
W25Q64 init rc=0, JEDEC 0xef4017
OSPI firmware check: PASS - booting (SP=0x20020000, Reset=0x90000841)
jumping to 0x90000000 ...
```

FAIL path (no / bad firmware in the W25Q64):

```
OSPI firmware check: FAIL (SP=0x00000000, Reset=0x00000000)
no bootable firmware on SPI flash - waiting (LED PG7 blinks)
boot FAIL: ...
```

## Writing a stage-2 firmware into the W25Q64

The firmware image is linked at `0x90000000` (see `../app_qspi/`). To write it
to the W25Q64:

- **probe-rs custom OCTOSPI flash algorithm** (`../qspi_map/algo/`): any `_qspi`
  app's `ninja flash` target does this. ✅ **Verified on hardware**: the write
  path works, and this bootloader boots `blink_hello_qspi` / `dhry_550m_qspi`
  from the W25Q64 at 550 MHz (`OSPI firmware check: PASS - booting`).
- **openocd `stmsmi`** — see `../qspi_map/README.md`.

## Notes

- The MPU is configured so `0x90000000..0x9FFFFFFF` is cacheable + executable
  (needed to run code out of the OSPI), matching the `qspi_map` bootloader.
- Stage-2 apps must not reconfigure the RCC/OSPI clocks (the bootloader owns
  them) and should use a non-destructive `SystemInit` — see
  `../app_qspi/*/src/system_app.c`.
