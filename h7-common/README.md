# h7-common — content shared by every STM32H7 board in this repo

Board-independent, chip-level ("H7 series") material lives here, so the boards do
not each carry their own copy. Everything board-specific stays with its board:
see [`h723-mini/`](../h723-mini/README.md) and [`h750-mini/`](../h750-mini/README.md).

## `drivers/` — STM32H7 HAL + CMSIS

One canonical driver tree, taken from ST's `STM32Cube_FW_H7_V1.13.0` package
(`%USERPROFILE%\STM32Cube\Repository\STM32Cube_FW_H7_V1.13.0\Drivers`), trimmed
to what the boards in this repo need:

* `STM32H7xx_HAL_Driver/Inc/` — all HAL headers.
* `STM32H7xx_HAL_Driver/Src/` — the HAL modules the boards reference (the union
  of what h723-mini and h750-mini used before the merge, 28 modules). Copy more
  modules from the same package if a project needs them.
* `CMSIS/Include/` + `CMSIS/Device/ST/STM32H7xx/Include/` — the CMSIS core plus
  the device headers of the boards here (`stm32h723xx.h`, `stm32h750xx.h`) and
  the family header `stm32h7xx.h`. Headers for other H7 parts can be copied from
  the same package.

Each board layer points at this tree (`<BOARD>_DRV`, see
[`h723-mini/cmake/stm32h723_board.cmake`](../h723-mini/cmake/stm32h723_board.cmake)
and [`h750-mini/cmake/stm32h750_board.cmake`](../h750-mini/cmake/stm32h750_board.cmake));
each project still lists the HAL modules it compiles.

The h750-mini tree is byte-identical to the V1.13.0 package (verified), so its
behaviour is unchanged. h723-mini moved from its older CubeH7 copy (CMSIS
V1.10.3) to this tree — its benchmarks do not call the HAL in the timed loop and
its projects all build, but the board should be re-verified on hardware the next
time it is connected.

## `cmake/` — board-independent CMake modules

| Module | Purpose |
| ------ | ------- |
| `arm-none-eabi-toolchain.cmake` | GNU arm-none-eabi gcc toolchain (the default) |
| `armclang-keil-toolchain.cmake` | Keil AC6 armclang for C + GNU as/ld |
| `starm-clang-toolchain.cmake` | ST Arm Clang (STM32CubeIDE's LLVM) + LLD |
| `armclang-postproject.cmake` | restores the GNU-style link rule after `project()` |
| `armclang_force_wint_t.h`, `printf_rename.h` | armclang/newlib ABI shims |
| `tool-path.cmake` | `h7_tool_path()`: repairs MSYS-vs-cmd path flavours |
| `probe-select.cmake` | probe discovery (`probe-rs list`), `h7_probe_args/ready/hint`, the `probes` target and the `flash-stlink` / `flash-dap` / `flash-jlink` families |
| `objcopy-targets.cmake` | `.hex` / `.bin` targets |
| `stm32h750_custom.yaml`, `stm32h743_custom.yaml` | probe-rs chip definitions with a 16 KB `page_size` (internal-flash speed) |

Each board's `cmake/` folder keeps only board-level files (the board layer and
the board's flash targets) plus one-line wrappers that include these shared
modules, so project-level include paths stay stable.

## Deliberately *not* shared

Per board: the board layer (clock tree, pinout, linker script, startup file,
`syscalls.c`, `stm32h7xx_hal_conf.h`), the projects, the CubeMX/Keil project,
bootloaders and flash algorithms, board images and the board README. Sharing
these would couple boards that have genuinely different hardware.
