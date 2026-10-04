# h7-demo — STM32H7 firmware development repo

Firmware projects and tooling for STM32H7 boards, one board per top-level folder
— currently the **h723-mini** board (STM32H723ZGT6, 1 MB flash, 550 MHz
Cortex-M7). The repo is named `h7-demo` rather than after a single chip because
it is meant to host several H7 boards (`h723-mini/` is the first). Project-level
docs live in each project folder; the board-level overview, clock tree, hardware
pinout and build/flash instructions are in [`h723-mini/README.md`](h723-mini/README.md).

## Chip

* **STM32H723ZGT6** — Cortex-M7 @ up to 550 MHz, single core, FPU/DSP.
* Flash **1 MB**; RAM: DTCM 128 KB (`0x20000000`), ITCM 64 KB (`0x00000000`),
  AXI SRAM 512 KB (`0x24000000`), D2 SRAM 288 KB (`0x30000000`).
* Clocks: HSE 25 MHz → PLL1 → SYSCLK 550 MHz (HCLK 275 MHz, APB 137.5 MHz,
  VOS scale 0, flash latency 3).
* Debug/flash: SWD via **ST-Link V2** (`ninja flash`, probe-rs) with USB DFU
  as fallback (`ninja dfu-flash`).

## Repo map

| Path             | What it is                                        |
| ---------------- | ------------------------------------------------- |
| `h723-mini/`     | Board + all projects (see its README)             |
| `h723-mini/bare/` | Bare-metal apps in internal flash (blink, benchmarks, LCD, flash test) |
| `h723-mini/app_qspi/` | The same apps linked at `0x90000000`, booted from the on-board W25Q64 |
| `h723-mini/tool/`| Bootloader, two-stage QSPI boot + flash algorithm, algorithm harness |
| `h723-mini/board/` | Shared board layer (clock, MPU, UART, startup, linker) |
| `h723-mini/board_images/` | Board layout / photo images               |
| `h723-mini/cubemx_file/` | Board CubeMX project (`.ioc`)             |
| `h723-mini/cmake/` | Toolchain + board + flash-target helpers        |
| `h723-mini/drivers/` | STM32H7 HAL + CMSIS (from the vendor example)  |
| `tools/`         | Cross-project helper scripts                     |

## Highlight: boot from the on-board W25Q64

The repo runs stage-2 apps **entirely from the external W25Q64** at the OCTOSPI
memory-mapped base `0x90000000`:

1. `h723_boot` (internal flash) initializes the W25Q64 and memory-maps it.
2. `ninja flash` on any `app_qspi/<app>` programs it into the W25Q64 through a
   custom **probe-rs OCTOSPI flash algorithm** (`tool/qspi_map/algo/`).
3. On reset the bootloader checks + jumps; the app runs at 550 MHz from
   external flash with the same benchmark scores as internal flash.

## Toolchain / environment

* GNU arm-none-eabi-gcc (default) or Keil AC6 armclang (`-DSTM32_TOOLCHAIN=armclang`).
* CMake + Ninja (MSYS2 mingw64 — `build.sh` adds it to `PATH`).
* probe-rs (SWD flashing), STM32CubeProgrammer (USB DFU fallback),
  pyserial (`tools/serial_capture.py`).

Build + flash any project with:

```bash
cd h723-mini/bare/dhry_550m
bash build.sh
ninja flash
```

## Tools (`tools/`)

* `serial_capture.py` — capture the USART console (cross-platform, pyserial).
