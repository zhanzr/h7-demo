# Embedded-flash vs QSPI (SPI-flash) project â€?difference + practical conversion guide

The h750-mini has two flavours of the *same* benchmark/app (`bare/dhry_480m` vs `app_qspi/dhry_480m`, etc.): the identical
sources, but the code is linked for and booted from a different flash.

## The differences

| | Embedded-flash app | QSPI app |
|---|---|---|
| **Link address** (linker script) | `board/stm32h750vbt.ld` â€?FLASH @ `0x08000000` | `app.ld` â€?QSPI (rx) @ `0x90000000`, 8 MB |
| **Vector table / VTOR** | `0x08000000` (chip boots it directly) | `0x90000000`; `h750_boot` sets `SCB->VTOR` before jumping |
| **Who owns the clock tree** | the app: `main()` calls `SystemClock_Config` (HSEâ†’PLL1â†?80 MHz) | the **bootloader**; app uses a non-destructive `SystemInit` and does NOT call `SystemClock_Config` (re-initing RCC would kill the PLL2â†’QUADSPI kernel and fault the next fetch) |
| **System init source** | stock `board/system_stm32h7xx.c` | per-project `src/system_app.c` (leaves RCC/PLL2/QSPI untouched) |
| **Flashing** | internal flash (`cmake/flash-targets.cmake`) | W25Q64 via the QUADSPI algorithm (`cmake/qspi-flash-targets.cmake`) |
| **Boot** | chip resets straight into the app | chip boots `h750_boot`, which checks + memory-maps the W25Q64 and jumps |

The benchmark/driver sources (`dhry_1.c`, `dhry_2.c`, `utils.c`, â€? are the
same in both twins â€?only the three switches below differ.

## Converting one to the other (temporarily)

**No application-code edit is needed.** The board layer (`board/board.c`) is
QSPI-aware: when a project is compiled with `QSPI_APP=1`, its
`SystemClock_Config()` is an empty no-op (the bootloader owns the clock tree),
`Board_Init()` skips `MPU_Config()` (the board MPU would deny access to
`0x90000000`) and calls `__enable_irq()` (h750_boot disables IRQ before
jumping). So both twins share the **identical `main()`** (`HAL_Init();
Board_Init();`). Conversion is pure CMake:

### Embedded-flash â†?QSPI (test the same app from SPI flash)

1. **Point at the QSPI linker + system-init files** â€?add these two lines
   *before* `add_executable`:

   ```cmake
   set(H750_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app.ld" CACHE FILEPATH
       "QSPI app linker script (code at 0x90000000)" FORCE)
   set(H750_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/system_app.c" CACHE FILEPATH
       "QSPI app system init (non-destructive)" FORCE)
   ```

2. **Provide the two files** â€?`app.ld` (code @ `0x90000000`) and
   `src/system_app.c` (non-destructive `SystemInit`, leaves RCC/PLL2/QSPI
   untouched). If either is not present, copy it from another `app_qspi/` project.
   List `src/system_app.c` in `add_executable`.

3. **Add the QSPI switch** (after `stm32h750_apply_board`):

   ```cmake
   target_compile_definitions(${PROJECT_NAME}.elf PRIVATE QSPI_APP=1)
   ```

   This alone makes `SystemClock_Config()`/`MPU_Config()` no-ops and re-enables
   IRQ â€?`main()` stays byte-identical to the embedded twin.

4. **Swap the flashing method** to:

   ```cmake
   include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)
   ```

   Then `ninja flash` writes the W25Q64 (prerequisite: `h750_boot` in internal
   flash).

### QSPI â†?Embedded-flash (test the same app from internal flash)

1. **Drop the two overrides** to use the board defaults (`stm32h750vbt.ld` +
   stock `system_stm32h7xx.c`). **Because the `set(... FORCE)` wrote the QSPI
   values into the CMake cache, reconfigure from a clean build dir**
   (`rm -rf build && cmake`) or the stale QSPI linker/system values persist.

2. **Remove `src/system_app.c`** from `add_executable` (and the file), and
   remove the `target_compile_definitions(... QSPI_APP=1)` line â€?`main()`
   itself needs no change.

3. **Swap the flashing method** back to:

   ```cmake
   include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
   ```

   Then `ninja flash` writes internal flash.

The easiest practical path is usually to **copy the twin project's folder and
edit only these CMake lines** â€?the sources (including `main()`) already match.
