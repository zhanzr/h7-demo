# h7-demo — STM32H7 firmware development repo

Firmware projects and tooling for STM32H7 boards: **one folder per board**, with
the chip-level (H7 series) content shared from [`h7-common/`](h7-common/README.md).

| Board | MCU | Clock | Console | Debug / flash | Docs |
| ----- | --- | ----- | ------- | ------------- | ---- |
| **h723-mini** | STM32H723ZGT6, 1 MB flash | 550 MHz | USART1 via CH340 (`COM89`) | ST-Link V2 (probe-rs), USB DFU fallback | [README](h723-mini/README.md) |
| **h750-mini** | STM32H750VBT6 (shadow H743 die, 2 MB flash) | 480 MHz | USART1 via CH340 (`COM89`/`COM56`) | ST-Link or ULINK2 as CMSIS-DAP (probe-rs) | [README](h750-mini/README.md) |

Every board folder has the same layout: `bare/` (internal-flash applications),
`app_qspi/` (the same applications linked at `0x90000000` and booted from the
on-board W25Q64 by a small bootloader), `board/` (clock, pinout, startup, linker
script), `cmake/` (board layer + flash targets), `tool/` (bootloader, QSPI flash
algorithm, host helpers) and a README with the hardware details, clock tree,
benchmarks and build/flash instructions.

Nothing chip-level is duplicated per board: the STM32H7 HAL + CMSIS drivers, the
generic CMake toolchain/probe modules and the probe-rs chip definitions live in
[`h7-common/`](h7-common/README.md).

## Toolchains

* **GNU arm-none-eabi-gcc** — the default; every project has its own `build.sh`.
* **armclang** (Keil MDK AC6) and **starm-clang** (ST Arm Clang from
  STM32CubeIDE, LLVM + LLD) — selectable in the benchmark projects with
  `-DSTM32_TOOLCHAIN=<gcc|armclang|starm-clang>`; use a separate build dir per
  toolchain (`CMAKE_TOOLCHAIN_FILE` is cached).
* Flashing uses **probe-rs**: `ninja flash` auto-detects the attached probe, and
  `flash-stlink` / `flash-dap` / `flash-jlink` force one. h723-mini additionally
  provides `ninja dfu-flash` (USB DFU fallback).

```bash
cd h723-mini/bare/dhry_550m && bash build.sh && ninja flash
cd h750-mini/bare/dhry_480m && bash build.sh && ninja flash
```

## Measured benchmarks

CoreMark 1.0 and Dhrystone 2.1 results — for gcc, armclang, starm-clang, from
internal flash and from QSPI — are recorded in each board's benchmark READMEs,
e.g. [`h723-mini/bare/coremark_550m`](h723-mini/bare/coremark_550m/README.md) and
[`h750-mini/bare/coremark_480m`](h750-mini/bare/coremark_480m/README.md).

## Tools

`tools/serial_capture.py` (console capture) and `tools/bench_capture.sh` (flash
+ capture) are shared by both boards. Board-specific helper scripts (build/flash
wrappers, openocd configs, QSPI UART download) live in each board's `tool/`.

If `ninja` fails with `CreateProcess failed: The system cannot find the file
specified.` while re-running CMake, the build dir was configured by the MSYS
cmake and is being driven by the native mingw64 `ninja` — see the troubleshooting
note in either board README (`build.sh` now prefers `/mingw64/bin`).
