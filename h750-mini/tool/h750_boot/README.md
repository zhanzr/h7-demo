# h750_boot — minimal STM32H750 bootloader (internal flash)

A small bootloader for the h750-mini (STM32H750VBT6) that lives in the internal
flash. On every reset it:

1. brings up the 480 MHz clock tree + USART1 console + LED,
2. initializes the W25Q64 (8 MB) via the QUADSPI,
3. performs a **basic** bootability check on a stage-2 firmware at the QSPI
   memory-mapped base `0x90000000`:
   - word 0 (initial stack top) must land in DTCM/SRAM `0x20000000..0x24000000`,
   - word 1 (reset vector) must be a thumb pointer inside `0x90000000..0x90800000`,
4. if the check **passes**: enters QUADSPI memory-mapped mode and jumps to the
   firmware,
5. if the check **fails**: loops forever, toggling the PA8 LED and printing the
   check result once per second.

The bootloader deliberately does **not** program the SPI flash — writing a new
stage-2 firmware is done externally (see below).

## Build & flash

```bash
bash build.sh                        # cmake -G Ninja + ninja -> build/h750_boot.hex
cd build && ninja flash              # probe-rs -> internal flash (ULINK2, SWD)
```

The `h750_boot.hex` is a normal internal-flash image; nothing else needs to be
flashed to use it.

## Console output

PASS path:

```
=== h750_boot bootloader @ 480000000 Hz ===
W25Q64 init rc=0, JEDEC 0xef4017
QSPI firmware check: PASS - booting (SP=0x20020000, Reset=0x90000841)
jumping to 0x90000000 ...
```

FAIL path (no / bad firmware in the W25Q64):

```
QSPI firmware check: FAIL (SP=0x00000000, Reset=0x00000000)
no bootable firmware on SPI flash - waiting (LED PA8 blinks)
boot FAIL: SP=0x00000000 Reset=0x00000000 (need SP in DTCM + thumb reset in 0x90000000..0x90800000)
boot FAIL: ...
```

## Writing a stage-2 firmware into the W25Q64

The firmware image is linked at `0x90000000` (see `QSPI_APP_GUIDE.md`). To write
it to the W25Q64 from the host:

- **probe-rs custom flash algorithm** (recommended): the hardware-QUADSPI
  algorithm in `../qspi_map/algo/` writes the image directly to the flash
  (~5 s for 41 KB, self-verifying). Any `_qspi` app's `ninja flash` target does
  this automatically (auto-building the algorithm YAML if needed):

  ```bash
  cd ../dhry_480m/build && ninja flash
  ```

  Or manually:

  ```bash
  python ../qspi_map/algo/build_algo.py flash_w25q64_qspi.c 0x4000
  probe-rs download --probe c251:2722:V0010M9E \
    --chip-description-path ../qspi_map/algo/target_w25q64_qspi.yaml \
    --chip STM32H750VB-W25Q64-w25q64_qspi --protocol swd \
    --binary-format hex --non-interactive --disable-progressbars \
    <app>.hex
  ```

  (Use `flash_w25q64_fast.c 0x1000` → `target_w25q64_fast.yaml` for the
  bit-banged fallback.)

  Do not pass `--verify` (probe-rs's external read-back path is unreliable;
  the algorithm self-verifies internally and the bootloader re-checks on boot).

- **bootloader-UART loader** (`../qspi_map` stage-1) or **openocd stmqspi** —
  see `../qspi_map/README.md`.

After writing, reset the board (or `probe-rs reset`) — h750_boot checks and
jumps.

## Notes

- The MPU is configured so `0x90000000..0x9FFFFFFF` is cacheable + executable
  (needed to run code out of QSPI), matching the `qspi_map` bootloader.
- Stage-2 apps must not reconfigure the RCC/PLL2/QSPI clocks (the bootloader
  owns them) and should use a non-destructive `SystemInit` — see
  `QSPI_APP_GUIDE.md`.
