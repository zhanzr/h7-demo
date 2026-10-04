# h7-demo — STM32H7 firmware development repo

Firmware projects and tooling for STM32H7 boards, **one board per top-level
folder**. Project-level docs live in each project folder; board-level overviews
(clock tree, pinout, benchmarks, build/flash instructions) live in each board's
`README.md`.

| Board | MCU | Core clock | Console | Debug/flash | Board docs |
| ----- | --- | ---------- | ------- | ----------- | ---------- |
| **h723-mini** | STM32H723ZGT6 (1 MB flash) | 550 MHz | USART1 via CH340 (`COM89`) | ST-Link V2 (probe-rs) + USB DFU fallback | [`h723-mini/README.md`](h723-mini/README.md) |
| **h750-mini** | STM32H750VBT6 (shadow H743 die, 2 MB flash) | 480 MHz | USART1 via CH340 (`COM56`) | Keil ULINK2 as CMSIS-DAP (probe-rs); openocd alternative | [`h750-mini/README.md`](h750-mini/README.md) |

Both boards use the same two-stage scheme: a small bootloader in internal flash
initializes the on-board W25Q64 and memory-maps it, and the stage-2 apps run
from external flash at `0x90000000` with (nearly) internal-flash speed.

## Repo map

| Path | What it is |
| ---- | ---------- |
| `h723-mini/` | STM32H723ZGT6 board + all of its projects |
| `h723-mini/bare/` | Bare-metal apps in internal flash (blink, CoreMark, Dhrystone, LCD, flash test) |
| `h723-mini/app_qspi/` | The same apps linked at `0x90000000`, booted from the on-board W25Q64 (OCTOSPI) |
| `h723-mini/tool/` | `h723_boot`, two-stage QSPI boot + flash algorithm, algorithm harness |
| `h723-mini/board/` | Shared board layer (clock, MPU, UART, startup, linker) |
| `h723-mini/board_images/` | Board layout / photo images |
| `h723-mini/cubemx_file/` | Board CubeMX project (`.ioc`) |
| `h723-mini/cmake/` | Toolchain + board + flash-target helpers |
| `h723-mini/drivers/` | STM32H7 HAL + CMSIS (from the vendor example) |
| `h750-mini/` | STM32H750VBT6 board + all of its projects |
| `h750-mini/bare/` | Bare-metal apps in internal flash (blink, benchmarks, camera→LCD, LCD, flash test, HSE test, 2 MB flash check) |
| `h750-mini/app_qspi/` | The same apps linked at `0x90000000`, booted from the on-board W25Q64 (QUADSPI) |
| `h750-mini/tool/` | `h750_boot`, two-stage QSPI boot + flash algorithm, harness, host helpers |
| `h750-mini/board/` | Shared board layer (480 MHz clock, USART console, startup, linker) |
| `h750-mini/board_images/` | Board photo |
| `h750-mini/cubemx_file/` | CubeMX/Keil project — also this board's HAL/CMSIS source (no separate `drivers/`) |
| `h750-mini/cmake/` | Toolchain + board + flash-target helpers, custom probe-rs chip definitions |
| `tools/` | Cross-project helper scripts (`serial_capture.py`, `bench_capture.sh`) |

## Highlight: boot from the on-board W25Q64

Both boards run stage-2 apps **entirely from the external W25Q64** at the
memory-mapped base `0x90000000`:

1. `h723_boot` / `h750_boot` (internal flash) initialize the W25Q64 and
   memory-map it.
2. `ninja flash` on any `app_qspi/<app>` programs it into the W25Q64 through a
   custom **probe-rs flash algorithm** (`<board>/tool/qspi_map/algo/`).
3. On reset the bootloader checks + jumps; the app runs at full speed from
   external flash with essentially the same benchmark scores as internal flash.

The h723-mini board uses OCTOSPI; h750-mini uses QUADSPI (hardware QUADSPI
algorithm, ~5 s for a 41 KB image). See each board README for the bootability
rules and the algorithm details.

## Toolchains / environment

* **GNU arm-none-eabi-gcc** — the default everywhere (`build.sh` in each project).
* **armclang** (Keil MDK AC6) — `-DSTM32_TOOLCHAIN=armclang` on both boards
  (separate build dir, e.g. `build-ac6/`); h750-mini also has the Keil uVision
  project in `h750-mini/cubemx_file/MDK-ARM/`.
* **starm-clang** (ST Arm Clang, from STM32CubeIDE) — implemented for the four
  h723-mini benchmark projects (`-DSTM32_TOOLCHAIN=starm-clang`); see
  `h723-mini/bare/coremark_550m/README.md` for the measured results.
* CMake + Ninja (MSYS2 mingw64 — `build.sh` adds it to `PATH`).
* probe-rs (SWD flashing), STM32CubeProgrammer (USB DFU fallback on h723-mini),
  pyserial (`tools/serial_capture.py`).

Build + flash any project with:

```bash
# h723-mini (ST-Link V2 / probe-rs)
cd h723-mini/bare/dhry_550m && bash build.sh && ninja flash

# h750-mini (ULINK2 as CMSIS-DAP / probe-rs)
cd h750-mini/bare/dhry_480m && bash build.sh && ninja flash
```

## Tools (`tools/`)

* `serial_capture.py` — capture the USART console (cross-platform, pyserial).
* `bench_capture.sh` — flash a benchmark image and capture its console output
  (`PORT=... CHIP=... PROBE=...` overrides; used by both boards).

Board-specific helper scripts (build/flash wrappers, openocd configs, QSPI UART
download) live in each board's `tool/` folder.
