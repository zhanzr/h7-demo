# cubemx_file — CubeMX/Keil project (ov5640_to_st7789_keil)

Keil MDK / ARMCLANG V6.24 variant of the h750-mini app (OV5640 camera ->
ST7789 240x320 SPI LCD). Board/chip-level info: see [`../README.md`](../README.md).

> ⚠ **Regenerating from CubeMX (`stm32h750_prj.ioc`) has consequences — read
> this before pressing "Generate Code".** CubeMX treats this as a standalone
> project, so a regeneration will:
>
> 1. **recreate a local `Drivers/` folder** here. The HAL/CMSIS sources now live
>    in the shared [`../../h7-common/drivers`](../../h7-common/README.md) tree
>    that every board in the repo builds against, so the local copy would be a
>    second, silently diverging HAL.
> 2. **rewrite `MDK-ARM/stm32h750_prj.uvprojx`**, dropping the
>    `../../../h7-common/drivers/...` include and file paths and pointing back
>    at `../Drivers/...`.
> 3. **overwrite `Core/Src/*` and `Core/Inc/*`**, losing the manual edits that
>    [`BOOT_FIXES.md`](BOOT_FIXES.md) and `post_cubemx_restore.ps1` describe.
>
> The CMake builds never read `cubemx_file/Drivers`, so the firmware would keep
> building — but the Keil project would compile a stale copy of the HAL and the
> board would quietly drift away from the shared drivers. If you do regenerate:
> delete the regenerated `Drivers/`, re-run `post_cubemx_restore.ps1`, and
> re-point the `.uvprojx` paths at `../../../h7-common/drivers/`
> (`git diff h750-mini/cubemx_file` shows exactly what changed).

**The HAL/CMSIS sources used by the CMake projects are shared** (repo-level
`h7-common/drivers`, see [`../../h7-common/README.md`](../../h7-common/README.md)):
this folder keeps the CubeMX/Keil project itself — `Core/` (application code and
the board's `stm32h7xx_hal_conf.h`), `MDK-ARM/` and the `.ioc`.

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
USART1 (PA9/PA10) @ 115200 -> on-board CH340 (`COM89`). `COMPILER_NAME` in
`Core/User/custom_def.h` prints `ARMClang <ver>` so the Keil build is
identifiable in the serial log.

## History
Old Dhrystone 2.1 (480 MHz, ARMCLANG -Omax, FPU) benchmark notes were kept
here before the project became the camera->LCD app; source `Core/User/dhry_*.c`
is no longer built into the project.
