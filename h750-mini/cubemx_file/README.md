# cubemx_file — CubeMX/Keil project (ov5640_to_st7789_keil)

Keil MDK / ARMCLANG V6.24 variant of the h750-mini app (OV5640 camera ->
ST7789 240x320 SPI LCD). Board/chip-level info: see [`../README.md`](../README.md).

**This folder is also the shared driver source for every CMake project on this
board**: the h750-mini board layer (`../cmake/stm32h750_board.cmake`,
`H750_ROOT`) and the two camera projects compile
`cubemx_file/Drivers/STM32H7xx_HAL_Driver` + `cubemx_file/Drivers/CMSIS`
directly. The h723-mini board keeps that HAL in a separate `drivers/` folder
instead; here it stays inside the CubeMX project so CubeMX regeneration and the
Keil project keep working unchanged.

## Layout
- `stm32h750_prj.ioc` — CubeMX source of truth (regenerate `Core/Src`,
  `Core/Inc`, `Drivers`).
- `MDK-ARM\stm32h750_prj.uvprojx` — Keil project.
- `post_cubemx_restore.ps1` — reapplies manual edits after CubeMX regen
  (see also `BOOT_FIXES.md`).
- `Core\User\` — app code (LCD `lcd_spi_200.*`, OV5640 `dcmi_ov5640.*`,
  SCCB `sccb.*`, fonts, utils, `custom_def.h`).

## Build
```sh
D:/Keil_v5/UV4/UV4.exe -j0 -b MDK-ARM/stm32h750_prj.uvprojx
```

## Memory
DTCM @ `0x20000000` (stack + heap + RW/ZI); camera DMA buffer in AXI SRAM
@ `0x24000000` (`Camera_Buffer`). 128 KB flash.

## Console
USART1 (PA9/PA10) @ 115200 -> on-board CH340 (`COM56`). `COMPILER_NAME` in
`Core/User/custom_def.h` prints `ARMClang <ver>` so the Keil build is
identifiable in the serial log.

## History
Old Dhrystone 2.1 (480 MHz, ARMCLANG -Omax, FPU) benchmark notes were kept
here before the project became the camera->LCD app; source `Core/User/dhry_*.c`
is no longer built into the project.
