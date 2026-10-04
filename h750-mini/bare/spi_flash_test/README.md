# spi_flash_test

W25Q64 (8 Mbyte) QUADSPI flash benchmark + XIP demo for the h750-mini board
(STM32H750VBT6 @ 480 MHz, W25Q64 on QUADSPI bank 1).

Measures, printed over USART1 @ 115200 (COM89), re-run every second:

- **erase** (line-mode independent, always 1-1-1): 4K sector / 32K block / 64K block
- **write** (page program): 1-1-1 (0x02) vs 1-1-4 (0x32)  (W25Q64 has **no**
  2-line page-program command, so only 1 and 4 data-line writes exist)
- **read, indirect HAL** (FIFO polling): 1-1-1 / 1-1-2 / 1-2-2 / 1-1-4 / 1-4-4
- **read, memory-mapped** (0x90000000 + offset): same 5 line modes
- **XIP**: copies a 16-byte position-independent function into flash sector 0
  and executes it from the mapped space

Every read/write is checksum-verified against the source pattern, so a
`FAIL` means genuinely corrupt data, not just a slow link.

## Measured results (this board, 100 MHz QSPI clock)

| operation              | result                             |
|------------------------|------------------------------------|
| erase 4K  sector       | ~53 ms                             |
| erase 32K block        | ~117 ms                            |
| erase 64K block        | ~189 ms                            |
| write 64K, 1-1-1       | ~114 ms  (~562 KiB/s)              |
| write 64K, 1-1-4       | ~112 ms  (~569 KiB/s)              |
| read 256K, HAL poll    | ~10.35 MiB/s in **all** 5 modes    |
| read 256K, memmap 1-1-1| 11.92 MiB/s                        |
| read 256K, memmap 1-1-2| 23.84 MiB/s                        |
| read 256K, memmap 1-2-2| 23.84 MiB/s                        |
| read 256K, memmap 1-1-4| 47.68 MiB/s                        |
| read 256K, memmap 1-4-4| 47.68 MiB/s                        |
| XIP execute            | OK (correct result)                |

Notes:

- **Write speed is identical for 1-1-1 and 1-1-4.** A page program is
  dominated by the flash's internal program time (~0.4 ms/page), not the wire
  transfer; at 100 MHz the data-shift is a few microseconds per page. Do not
  expect quad writes to be 4x faster - only the transfer phase is faster.
- **HAL polling reads are CPU-bound.** `HAL_QSPI_Receive` drains the FIFO one
  byte at a time with a flag poll per byte, so 1-line and 4-line reads measure
  the same ~10 MiB/s. Use memory-mapped mode (or DMA/MDMA) for real throughput.
- **Memory-mapped reads scale exactly with line count**: 1x / 2x / 4x
  (11.92 / 23.84 / 47.68 MiB/s), with the D-cache enabled (see `mpu_qspi_config`).
- Erase times match the W25Q64JV datasheet ranges.

## Build + flash

```
bash build.sh                 # CMake + Ninja, GNU arm-none-eabi-gcc
ninja                         # builds spi_flash_test.elf + .hex
ninja flash                   # probe-rs, STM32H750VB (image is < 128 KB)
```

or with the capture harness:

```
bash tools/bench_capture.sh h750-mini/spi_flash_test/build/spi_flash_test.hex 16 spi
```

## Driver design (`src/w25q64.c/.h`)

- **QSPI clock = 100 MHz** (PLL2: M25/N400/R2 -> 200 MHz, prescaler 1).
  125 MHz works but 1-line reads (0x03) become intermittently corrupt on this
  board (single-byte bit errors); 100 MHz is rock solid. The W25Q64JV is rated
  133 MHz, the STM32H750 QUADSPI spec is 100-133 MHz depending on
  configuration - 100 MHz leaves margin.
- **Quad Enable (QE) is set during init.** The W25Q64JV ships with
  SR2.QE = 0, and without it all quad pins are disconnected and quad commands
  silently fail (0xEB reads return garbage, 0x32 writes program nothing).
  QE is a non-volatile SR2 bit, and non-volatile status writes are gated by
  WP# (IO2). The driver:
  1. reads SR2,
  2. drives IO2/PE2 high (WP# = 1) as a GPIO during the write,
  3. writes SR2 |= 0x02 - tries the 1-byte `0x31` form first, falling back to
     the 2-byte `0x01` (SR1,SR2) form,
  4. verifies by re-reading SR2.
- **Continuous-read mode is exited explicitly.** For the I/O reads (1-2-2
  0xBB, 1-4-4 0xEB) the W25Q64 interprets the first dummy cycles as mode
  bits; if they read as `0xA0` the device *enters* continuous-read mode and
  stops decoding normal commands (erases/writes fail afterwards). The driver
  sends `0xF0` mode bits via the HAL alternate-byte mechanism
  (`QSPI_ALTERNATE_BYTES_2_LINES`/`_4_LINES`, 8 bits) so the chip always exits
  continuous mode. Dummy counts: 0x03=0, 0x3B=8, 0xBB=4(as mode bits), 0x6B=8,
  0xEB=6(2 mode + 4 dummy).
- **Memory-mapped mode uses the timeout counter** (`QSPI_TIMEOUT_COUNTER_ENABLE`,
  period 0x10). With it disabled the peripheral's BUSY flag never clears after
  a memmap transaction, so the first indirect command after `memmap_stop`
  blocks forever (HAL waits for BUSY=0). `memmap_stop` also forces
  `hqspi.State = HAL_QSPI_STATE_READY` then issues a 1-line status command to
  switch FMODE back to indirect.

## Vendor driver findings (`D:\board_database\main-stm32h750-cam\spi_w25q64\`)

The reference CubeMX/Keil project (`Drivers/User/Src/qspi_w25q64.c`) was
reviewed against the hardware. Confirmed issues:

1. **It never sets SR2.QE.** It uses 0xEB (1-4-4) reads and 0x32 (1-1-4)
   writes directly, but never enables quad mode. On a factory-fresh W25Q64JV
   those quad commands cannot work. (Either their board shipped pre-programmed
   with QE=1, or quad mode never actually functioned.)
2. It has no memory-mapped exit path (its `QSPI_W25Qxx_MemoryMappedMode`
   enters and never leaves).
3. Its HAL error handling mostly returns bare error codes without detail.

The vendor's memory-mapped config (0xEB, 6 dummy) does match the STM32
convention and works once QE is set.

## XIP / code-in-mapped-space research

- QUADSPI bank1 memory-mapped base = **0x90000000** (bank2 = 0x70000000).
- On STM32H750 the QUADSPI memory-mapped mode is the only way to execute code
  from external SPI flash (OCTOSPI exists only on H7A3/H7B3).
- The firmware demonstrates this by writing a small position-independent
  function (`src/qspi_xip.S`, `.qspi_code` section, exported through
  `__qspi_code_start/__qspi_code_end` in the linker script) into flash and
  calling it at `0x90000000`. MPU region 0 marks 0x90000000..0x9FFFFFFF as
  normal, cacheable, **executable** (see `mpu_qspi_config()` in `main.c`).
- For a fully linked-to-QSPI image you would add a `QSPI (rx)` MEMORY region
  at 0x90000000 and a loadable section `AT> FLASH` (or a dedicated image for
  external-flash programming). The vendor's Keil `.sct` does not do this
  (128 KB internal flash only).

## Flashing the QSPI-mapped space (external-flash driver)

### Are different drivers needed for openocd vs probe-rs?

**Yes - completely different mechanisms**, even though both end up executing
code on the target CPU (only the MCU can reach the QUADSPI peripheral):

| tool      | mechanism | STM32 H7 external QSPI |
|-----------|-----------|------------------------|
| openocd   | C flash drivers compiled into the openocd binary, configured over TCL (`flash bank`) | `stmqspi` driver (`src/flash/nor/stmqspi.c`) drives the QUADSPI registers over the debugger and runs a small working-area algorithm on the target. Also `stmsmi` for OCTOSPI-class parts. Confirmed compiled into the installed xPack openocd 0.12.0 |
| probe-rs  | CMSIS-Pack flash algorithms: an ELF (`.FLM`-standard) that probe-rs downloads into target RAM and runs. Attached to a chip via a YAML target description | no W25Q64 algorithm bundled (see below); add a custom algorithm + `--chip-description-path` |

So one external-flash "driver" cannot be shared: openocd needs compiled-in C,
probe-rs needs an ELF flash algorithm embedded in a target YAML.

### Existing drivers on central sites

- **openocd**: `stmqspi` already understands the W25Q64 command set (read
  `0x03`, qread `0xEB/0xBB/0x6B/0x3B`, page program `0x02/0x32`, sector/block
  erase `0x20/0x52/0xD8`, chip erase `0xC7`). **No driver source changes are
  needed** - only a board TCL config that enables the QSPI clocks + GPIO and
  puts the QUADSPI in memory-mapped mode at `reset init`, then:
  ```
  flash bank qspi_bank stmqspi 0x90000000 0 0 0 $_CHIPNAME.cpu0 0x52005000
  stmqspi set qspi_bank w25q64 0x800000 256 0x03 0xEB 0x02 0xC7 0x1000 0x20
  ```
  The last flash-bank argument is the QUADSPI register base, which is
  **0x52005000** on STM32H7 (D1 AHB1; confirmed by `stm32h750xx.h` and
  OpenOCD's `board/stm32h7b3i-disco.cfg` which uses the same base for
  OCTOSPI1). `stmqspi set` args are: device name, size, page size, read cmd,
  quad-read cmd, page-program cmd, chip-erase cmd, sector size, sector-erase cmd.
- **probe-rs**: the `STM32H750VB` target (probe-rs 0.32.0,
  `targets/STM32H7_Series.yaml`) already lists external QSPI flash algorithms -
  `mt25tl01g_stm32h750b-disco`, `stm32h7xx_mt25tl01g`, `mtfc4gacajcn_stm32h750b-disco`,
  `mx25lm51245g_stm32h7b3i-*` - and probe-rs auto-synthesizes an NVM region at
  0x90000000 from their address ranges (that is why `probe-rs chip info` shows
  `NVM 0x90000000..0x98000000`). None of those algorithms matches a W25Q64, and
  several overlap, so flashing our chip would select the wrong command set (or
  fail with "multiple algorithms, no default"). Community W25 algorithms exist
  as references (e.g. `MakerPnP/dev-tools` W25Q16JV, `pkoevesdi/STM32h750xx_MT25QL512ABB`).

### Is writing our own feasible?

- **openocd**: easiest - `stmqspi` is already built in, write only the board
  init TCL (~30 lines of register writes for RCC/GPIO/QUADSPI + memory-mapped
  mode), then use the normal `program ... verify reset exit` flow. Caveat: the
  driver's commands are issued with the target **halted**; it reads/writes the
  QUADSPI registers directly via the debugger and uses on-target code for bulk
  read/write/verify.
- **probe-rs**: write a CMSIS flash algorithm (Rust via the
  `probe-rs/flash-algorithm-template` crate, or C/assembly modeled on an ST
  `.FLM`), base64-embed it into a custom YAML for STM32H750VB with an `!Nvm`
  region at 0x90000000 (8 MB), then `probe-rs download --chip-description-path
  my-target.yaml`. The algorithm must (a) init QUADSPI clocks/GPIO, (b) set
  SR2.QE (this project's driver already solved the two pitfalls: WP#/IO2 must
  be high for the non-volatile status write, and quad I/O reads must send
  `0xF0` mode bits to exit continuous-read mode), (c) program via 0x02/0x32 and
  erase via 0x20/0x52/0xD8. Moderate effort; the flash-algorithm-template makes
  the ELF generation turnkey.

### 4.1 How to map the SPI flash into the code space

Put the QUADSPI in **memory-mapped mode**: the flash byte at offset N becomes
readable/executable at `0x90000000 + N` (bank1). In practice: program the CCR
with the XIP read command (e.g. 0xEB 1-4-4), set `FMODE = memory-mapped`, enable
the peripheral, and mark `0x90000000..0x9FFFFFFF` executable + cacheable in the
MPU (as `mpu_qspi_config()` does in `main.c`). The H750 cannot boot from QSPI -
the reset vector must come from internal flash, which initializes QSPI and then
jumps to 0x90000000.

### 4.2 Is QSPI the only supported way to map it?

On the STM32H750, **yes** - QUADSPI memory-mapped mode is the only peripheral
that can put an external SPI flash into the CPU code space. (OCTOSPI only exists
on H7A3/H7B3; the FMC maps parallel NOR, not SPI. SPI/I2S cannot map memory.)
So "map the W25Q64 into code space" == "use QUADSPI memory-mapped mode".

### 4.3 How to modify the linker script for the mapped space

Add a `QSPI (rx)` MEMORY region at 0x90000000 and place code/data there:

```
MEMORY {
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 128K
  QSPI  (rx) : ORIGIN = 0x90000000, LENGTH = 8M
}
SECTIONS {
  .qspi_text : { *(.qspi_text) } > QSPI
}
```

Two loading styles:
- **Bootloader scheme**: `.qspi_text : { ... } > QSPI AT> FLASH` - the image is
  loaded into internal flash (0x08000000 region) and copied to QSPI at runtime
  by the bootloader before jumping. This is what the `spi_flash_test.ld`
  `.qspi_code` section demonstrates (it keeps the bytes in internal flash).
- **Direct scheme**: link the image so it *starts* at 0x90000000 and generate a
  binary whose load address is the QSPI space, then program it with an external
  flash driver. `arm-none-eabi-objcopy -O binary` with `--change-addresses` or
  a dedicated linker script + HEX at 0x90000000.

### 4.4 How to flash the mapped space (openocd / probe-rs first, Keil later)

1. **openocd** (recommended first): reuse the installed `stmqspi` driver. Write
   a board config that brings the QUADSPI to memory-mapped mode at `reset init`,
   create the flash bank as above, then `openocd -f ... -c "program
   image.hex verify reset exit"` where `image.hex` carries addresses in
   0x90000000.
2. **probe-rs**: supply a W25Q64 flash algorithm + custom target YAML, then
   `probe-rs download --chip-description-path my-target.yaml --chip STM32H750VB
   --binary-format hex image.hex`.
3. **Keil**: deferred. Keil uses `.FLM` external flash loaders (ST's `.stldr`
   for STM32CubeProgrammer is a different format); an ST-Link + Keil FLM for the
   W25Q64 would be written for the FLM API.
