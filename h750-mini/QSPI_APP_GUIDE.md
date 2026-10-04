# Creating a pure-QSPI app (code runs from the W25Q64)

A **pure-QSPI app** is a firmware whose code (.text/.rodata) is linked at the
QUADSPI memory-mapped base `0x90000000` and is executed straight out of the
on-board W25Q64. It is booted by a bootloader in internal flash
(`h750_boot`, or the `qspi_map` stage-1), which brings up the clocks, maps the
flash and jumps to the app's reset vector.

Naming convention: projects whose code space is QSPI live under **`app_qspi/`**, carrying the same project name as their embedded-flash twin under `bare/`
(e.g. `dhry_480m`, `coremark_480m`). The embedded-flash twins have no
suffix (`dhry_480m`, `coremark_480m`).

Reference implementations you can copy:
- `tool/qspi_map/bare/` â€?the minimal example app + `app.ld` + `src/system_app.c`
- `dhry_480m/` and `coremark_480m/` â€?benchmark ports
- `h750_boot/` â€?the bootloader that checks and jumps

---

## 1. What a QSPI app needs

Compared to a normal internal-flash firmware there are **exactly three**
changes:

| # | Piece                    | Internal flash                   | Pure QSPI                         |
|---|--------------------------|----------------------------------|-----------------------------------|
| 1 | Linker script            | FLASH @ 0x08000000               | QSPI @ 0x90000000 (8 MB)          |
| 2 | System init              | stock `system_stm32h7xx.c`       | non-destructive `system_app.c`    |
| 3 | `main()` init            | `Board_Init()` (owns clocks/MPU) | `HAL_Init(); UART_Init();` only   |

Everything else â€?the application/benchmark sources, board HAL, UART console â€?stays identical.

### 1.1 Linker script (`app.ld`)

Copy `tool/qspi_map/bare/app.ld`. Memory map:

```
MEMORY
{
  QSPI (rx)  : ORIGIN = 0x90000000, LENGTH = 8M   /* code + rodata + .data LMA */
  DTCM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K /* data + bss + stack        */
}
```

It is the same script the internal-flash port uses except the FLASH region is
replaced with the QSPI region at `0x90000000` (and `.data` is loaded from QSPI
to DTCM, like any normal C runtime init).

### 1.2 Non-destructive SystemInit (`system_app.c`)

The stock `SystemInit()` in `system_stm32h7xx.c` **resets the whole RCC clock
tree** (PLL1/PLL2/PLL3, HSE, dividers). That would kill the
PLL2â†’QUADSPI clock the bootloader configured â€?and because the app's *own*
code lives in QSPI, the next instruction fetch would fault.

Copy `tool/qspi_map/bare/src/system_app.c`: it provides `SystemInit()`/`ExitRun0Mode()`
that do nothing destructive, and a `SystemCoreClock` that reflects what the
bootloader set (480 MHz). The bootloader owns the clock tree; the app only uses
it.

Select it in the CMake build by forcing the shared `H750_SYSTEM_SOURCE` cache
var before `stm32h750_apply_board()`:

```cmake
set(H750_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app.ld"      CACHE FILEPATH "" FORCE)
set(H750_SYSTEM_SOURCE  "${CMAKE_CURRENT_SOURCE_DIR}/system_app.c" CACHE FILEPATH "" FORCE)
```

(`H750_LINKER_SCRIPT` / `H750_SYSTEM_SOURCE` are the two shared cache vars the
`stm32h750_board.cmake` layer reads; the `FORCE` is required so the value from a
previous target/configure run does not win.)

### 1.3 main() init

Do **not** call `Board_Init()` â€?it re-runs `SystemClock_Config()` (destructive)
and installs a 4 GB MPU that denies QSPI. The bootloader already configured
480 MHz + the QSPI-friendly MPU, so the app only needs:

```c
int main(void)
{
    HAL_Init();            /* SysTick only; clocks are owned by the bootloader */
    __enable_irq();        /* the bootloader jumped with PRIMASK set           */
    UART_Init();           /* console; does not touch PLL2/QUADSPI             */
    /* ...your app... */
}
```

`SystemCoreClock` is already 480000000 via `system_app.c`, so timers/UART/HAL
delay all work.

---

## 2. Flashing the app into the W25Q64

1. Build the QSPI app â†?produces `<name>.hex` (Intel HEX at `0x90000000`).
2. Write it to the W25Q64. **The easy way â€?every `_qspi` project has a
   `ninja flash` target** (from `cmake/qspi-flash-targets.cmake`) that builds
   the `.hex`, auto-detects the QUADSPI algorithm YAML in
   `tool/qspi_map/algo/` (runs `build_algo.py` only if it's missing or older
   than the algorithm source), then flashes the W25Q64 via probe-rs:

```bash
cd app_qspi/dhry_480m/build
ninja flash
```

   Prerequisite (one-time per board): `h750_boot` must be in internal flash.

   Manual variant (same thing, explicit):

   | Variant | Source / YAML | Speed (41 KB) | Use |
   | ------- | ------------- | ------------- | --- |
   | **qspi**   | `flash_w25q64_qspi.c` / `target_w25q64_qspi.yaml` | **~5 s** | **recommended (hardware QUADSPI)** |
   | fast   | `flash_w25q64_fast.c` / `target_w25q64_fast.yaml` | ~6 s | bit-banged SPI, fallback |
   | basic  | `flash_w25q64.c` / `target_w25q64.yaml` | ~14 s | slowest fallback |

   (The measured times use a 16 KB probe-rs page chunk for the QUADSPI variant
   and 4 KB for the bit-banged ones; the old 256-byte default cost ~10 s extra
   because probe-rs's per-page `ProgramPage` call overhead dominated, not SPI.)

   All are self-verifying (every page program + sector erase read back and
   retried). Build the recommended one and flash:

```bash
cd tool/qspi_map/algo
python build_algo.py flash_w25q64_qspi.c 0x4000   # -> target_w25q64_qspi.yaml
probe-rs download --probe c251:2722:V0010M9E \
  --chip-description-path target_w25q64_qspi.yaml \
  --chip STM32H750VB-W25Q64-w25q64_qspi --protocol swd \
  --binary-format hex --non-interactive --disable-progressbars \
  /path/to/<name>.hex
```

Do **not** pass `--verify` (probe-rs's external read-back path is unreliable;
the algorithm self-verifies internally instead).

### The QUADSPI-peripheral algorithm (solved)

`flash_w25q64_qspi.c` uses the hardware QUADSPI in 1-line indirect mode. It was
debugged to completion with the `probers_alg` harness project (a normal
firmware that runs the algorithm's exact register-level code with printf
visibility). **Three root causes were found and fixed:**

1. **GPIO AF bugs** â€?PB6 (NCS) is AF10 in `AFR[0]` bits 27:24 (was wrongly
   written to `AFR[1]`); PD11/PD12/PD13 (IO0/1/3) need `AFR[1]=0x00999000`
   (was shifted, setting PD9/10/11); all IO pins must be AF in `MODER`
   (PD11 was stuck as GPIO output).
2. **QUADSPI NCS is hi-Z when idle + PB6 has an external pull-down on this
   board** â†?the flash stayed permanently selected and entered continuous-read
   mode after the first command, so every later transfer failed. Fix: drive CS
   (PB6) **manually as a GPIO output** around each transfer (assert before the
   CCR/AR trigger, deassert after TCF), while the QUADSPI drives CLK + data.
3. **The DR FIFO must be accessed with 32-bit WORD reads/writes.** Byte
   accesses do not pop/push the FIFO (every byte read returns the same oldest
   byte). A word read pops 4 bytes (`byte0 = DR[7:0]`, ...).

With these, the algorithm runs **on HSI 64 MHz with no PLL at all** (the QSPI
kernel D1HCLK = HCLK = 64 MHz), QSPI clock ~16 MHz, and is fully
self-verifying. Verified end-to-end under probe-rs: 41 KB app flashed in ~5 s
and boots from QSPI (`JEDEC 0xef4017`, boot check PASS).

The earlier PLL1/PLL2/VOS-scale-0 findings are documented in the git history;
they are no longer needed for this algorithm.

The bit-banged fast driver (~6 s) and the ready `CLIVEONE` .FLM (Init +
EraseSector verified under probe-rs) remain as fallbacks.

3. Reset the board â€?`h750_boot` (or `qspi_map` stage-1) checks the image and
   jumps to it:

```bash
probe-rs reset --probe c251:2722:V0010M9E --chip STM32H750VB --protocol swd
```

---

## 3. Boot check performed by h750_boot

`h750_boot` only does a **basic** sanity check on the first 8 bytes of the W25Q64:

- word 0 (initial SP) in `0x20000000..0x24000000` (DTCM/SRAM), and
- word 1 (reset vector) a thumb pointer in `0x90000000..0x90800000`.

Any valid ARM image built with the `app.ld` above passes. If it fails, the
bootloader loops on the LED + prints the reason (see `tool/h750_boot/README.md`).

The bootloader puts the QUADSPI in memory-mapped mode with the **timeout counter
disabled** (`QSPI_TIMEOUT_COUNTER_DISABLE` â€?matching ST's `ExtMem_Boot`).
Keeping TCEN enabled lets a gap in memmap reads drop the `0x90000000` mapping
mid-run, which hard-faults large XiP workloads (this was the actual root cause
of the CoreMark-from-QSPI hang).

---

## 4. CMake skeleton for a QSPI (`app_qspi/`) project

```cmake
project(myapp C ASM)
set(CMAKE_C_STANDARD 11)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/stm32h750_board.cmake)

set(H750_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app.ld"       CACHE FILEPATH "" FORCE)
set(H750_SYSTEM_SOURCE  "${CMAKE_CURRENT_SOURCE_DIR}/system_app.c" CACHE FILEPATH "" FORCE)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/system_app.c
    # ...your app sources...
)
stm32h750_apply_board(${PROJECT_NAME}.elf "-Ofast")

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/objcopy-targets.cmake)
```

`ninja` produces `<name>.hex` at `0x90000000`; `ninja bin` produces the raw
`.bin`. Note `ninja flash` flashes to *internal* flash, so do not use it for a
QSPI app â€?use the algorithm command in section 2.

> Keil/MDK `.FLM` support is out of scope for now (the CMSIS `.FLM` format is
> the same as the probe-rs algorithm format, but no Keil project is shipped
> yet).
