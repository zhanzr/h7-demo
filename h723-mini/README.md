# h723-mini — STM32H723ZGT6 development projects

Firmware projects for the h723-mini board (STM32H723ZGT6 @ 550 MHz, USART1
console on PA9/PA10, LED on PG7 low-active, 1.54" 240x240 ST7789 LCD, ST-Link
V2 SWD probe). Ported from the h750-mini layout in `D:\stm32h750_prj\h750-mini`.

## Board (hardware)

* MCU: STM32H723ZGT6 (LQFP144, 1 MB flash, 550 MHz max).
* HSE: 25 MHz external crystal.
* LED: **PG7, low active** (`GPIO_PIN_RESET` = ON).
* USART1 console: PA9 (TX) / PA10 (RX), AF7, 115200 8-N-1.
* LCD: 1.54" ST7789 240x240 (SPI6 on PG13/PG14/PG15, PG12 backlight) — not
  used by these projects yet.
* Debug probe: **ST-Link V2 (SWD)** — the Keil ULINK2 (CMSIS-DAP v1) cannot
  access this H723's debug bus (DP reads, but every AP/core transaction fails;
  verified with Keil, probe-rs and OpenOCD), so don't use the ULINK2 here.

## Clock tree (550 MHz)

```
HSE 25 MHz → PLL1 (M=10, N=220, P=1) → SYSCLK 550 MHz
  AHB /2  → HCLK 275 MHz
  APB1/2/3/4 /2 → 137.5 MHz
  VOS scale 0 (1.35 V), flash latency 3
```

Copied verbatim from the vendor `1.LED闪烁` 550 MHz example. The board's own
`cubemx_file/cubemx_file.ioc` (16 MHz-HSE based PLL values) does NOT match
the 25 MHz crystal and is not used by these builds.

## Projects

The tree mirrors h750-mini: `app/` (embedded-flash applications), `board/`
(shared board layer), `cmake/` (toolchain/board helpers), `drivers/` (the
STM32H7 HAL + CMSIS pulled from the vendor example projects).

**`app/` (embedded flash):**
| Project          | What it is                                    |
| ---------------- | --------------------------------------------- |
| `blink_hello`    | LED blink (PG7) + UART (reference template)   |
| `dhry_550m`      | Dhrystone 2.1 benchmark @ 550 MHz             |
| `coremark_550m`  | CoreMark 1.0 @ 550 MHz                        |
| `st7789`         | 1.54" 240x240 ST7789 LCD demo (SPI6)          |
| `hse_test`       | HSE crystal check (boots on HSI 64 MHz)       |
| `spi_flash_test` | W25Q64 OCTOSPI flash benchmark + XIP demo     |

**`app_qspi/` (pure QSPI — code runs from the W25Q64 @ 0x90000000):**
| Project              | What it is                                  |
| -------------------- | ------------------------------------------- |
| `blink_hello_qspi`   | LED blink from the W25Q64                    |
| `dhry_550m_qspi`     | Dhrystone 2.1 from the W25Q64                |
| `coremark_550m_qspi` | CoreMark 1.0 from the W25Q64                 |
| `st7789_qspi`        | ST7789 LCD demo from the W25Q64              |

**`tool/` (boot + infra):**
| Project      | What it is                                             |
| ------------ | ------------------------------------------------------ |
| `h723_boot`  | Minimal bootloader: checks 0x90000000, memory-maps + jumps |
| `qspi_map`   | Two-stage boot + app + the probe-rs OCTOSPI flash algorithm |
| `probers_alg`| Harness: runs the OCTOSPI algorithm's register code as firmware |

Measured on this board (GCC 15.3.1, hard-float, I/D caches on, USART console):

| Benchmark          | Result                                  |
| ------------------ | --------------------------------------- |
| Dhrystone 2.1      | 2,631,579 D/s → **2.723 DMIPS/MHz**     |
| CoreMark 1.0       | **2372.59** (25,000 iters, ~10.5 s)     |

(550/480 MHz clock-scaled 1.146× from the h750-mini numbers, as expected.)

Verified on hardware:

* `st7789` drives the on-board 1.54" panel over SPI6 (PG8/13/14, 68.75 MHz SCK,
  DC PG15 — vendor `1.54寸240x240分辨率` pinout) with a **TIM23_CH1 PWM
  backlight on PG12** (`lcd_bl_bright_set`, h750-style brightness control), and
  loops shapes → pure colors → gradient → LED test with an on-screen FPS
  counter.
* `hse_test` reports `HSE: READY - crystal OK` (the 25 MHz HSE locks).
* `spi_flash_test` drives the on-board W25Q64 (8 MB) over OCTOSPI1 port 1
  (PF6-10, PG6, 137.5 MHz) — erase/write/read throughput in every line mode,
  memory-mapped reads up to **65.6 MiB/s** (1-4-4), and an **XIP** demo that
  executes code from `0x90000000` (all checksums OK).
* `h723_boot` (internal flash) runs at 550 MHz, initializes the W25Q64 via
  OCTOSPI, and boots a stage-2 app from `0x90000000` when present.

## QSPI / OCTOSPI status

✅ **Fully working**: the `_qspi` apps link at `0x90000000`, `ninja flash`
programs them into the W25Q64 via the **probe-rs OCTOSPI flash algorithm**
(`tool/qspi_map/algo/`), and `h723_boot` boots them at 550 MHz. Verified on
hardware: `blink_hello_qspi` (LED + console) and `dhry_550m_qspi`
(**2.722 DMIPS/MHz from external flash — identical to internal flash**).
The algorithm's write path (`ProgramPage`) was debugged with
`tool/probers_alg` and fixed — see `tool/qspi_map/algo/README.md` for the
root causes (FMODE = indirect write is `0b00`, command-only transfers
auto-start, wait for TCF).

The HAL/CMSIS under `drivers/` was copied from the vendor project
`board_database\main-stm32h723-mini\vendor_projects\1.LED闪烁` (CubeMX
STM32Cube FW_H7) — the three projects only exercise the RCC/GPIO/FLASH/PWR/
CORTEX/HSEM/UART HAL modules.

## Build & flash

Each project has `build.sh` (GNU arm-none-eabi-gcc, the default) and supports
a separate `build-ac6/` dir for Keil AC6 (armclang). The build outputs the
`.elf` + `.hex`; `ninja flash` programs the board via probe-rs + the ST-Link
V2, and `ninja dfu-flash` via USB DFU (fallback):

```bash
cd app/blink_hello
bash build.sh
ninja flash        # probe-rs download --chip STM32H723ZG via ST-Link V2 (SWD)
ninja dfu-flash    # USB DFU via STM32CubeProgrammer (needs BOOT0=1 + reset)
```

Then open the USART1 console at 115200 8-N-1 (on this PC: the ST-Link V2's
virtual COM port `COM46`, or the board's own USB-serial bridge).

> The `flash-targets.cmake` default probe selector
> `0483:3752:0672FF555054877567101040` is the ST-Link V2 serial on this PC;
> override with `-DDEBUG_PROBE=...` if it differs. The Keil ULINK2 cannot flash
> this board (CMSIS-DAP v1 cannot access the H723 core); use the ST-Link V2 or
> DFU.
