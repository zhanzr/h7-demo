# qspi_map: two-stage firmware that boots the STM32H723ZG from internal flash
# and runs the main app from the OSPI-mapped space (0x90000000).
#
#   boot.elf  - stage 1 bootloader in internal flash (1 MB). Initializes
#               clocks/UART/OSPI/W25Q64, checks + memory-maps the flash, and
#               jumps to 0x90000000.
#   app.elf   - stage 2 app, linked at 0x90000000 (OSPI memory-mapped). Runs
#               entirely from the W25Q64 (blink_hello-like).
#
# Targets:
#   ninja            - builds both .elf + .hex
#   ninja flash_boot - probe-rs -> boot.hex into internal flash (STM32H723ZG)
#   ninja flash      - alias for flash_boot
#   ninja bin        - app.raw .bin (for the bootloader UART download / ext flash)
#
# The stage-2 app is flashed into the W25Q64 separately:
#   * probe-rs custom OCTOSPI flash algorithm (see qspi_map/algo + app_qspi),
#   * openocd stmsmi, or
#   * the bootloader-UART download mode.
# See README.md.
#
# NOTE: the OSPI flash algorithm's write path is still under development
# (Init/read/erase verified on hardware; the TX-program path needs more work) -
# see qspi_map/algo/README.md.

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc (arm-none-eabi, default) or armclang (Keil AC6)")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang)
option(STM32_LTO "Enable -flto with the GCC toolchain" OFF)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(qspi_map C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h723_board.cmake)

# ---------------------------------------------------------------------------
# Stage 1: bootloader (internal flash)
# ---------------------------------------------------------------------------
set(H723_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/boot/boot.ld" CACHE FILEPATH
    "bootloader linker script (internal flash)" FORCE)
set(H723_SYSTEM_SOURCE "${BOARD_DIR}/system_stm32h7xx.c" CACHE FILEPATH
    "bootloader system init (stock, owns the clocks)" FORCE)

add_executable(boot.elf
    boot/src/main.c
    ../../app/spi_flash_test/src/w25q64.c
)
stm32h723_apply_board(boot.elf "-O2")
target_sources(boot.elf PRIVATE
    ${H723_HAL}/Src/stm32h7xx_hal_ospi.c
    ${H723_HAL}/Src/stm32h7xx_hal_mdma.c)
target_include_directories(boot.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app/spi_flash_test/src)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/boot.hex
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:boot.elf> ${CMAKE_CURRENT_BINARY_DIR}/boot.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:boot.elf>
    DEPENDS boot.elf
    COMMENT "objcopy -> boot.hex (Intel HEX)")
add_custom_target(boot_hex ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/boot.hex)

# ---------------------------------------------------------------------------
# Stage 2: OSPI app (0x90000000). Override system source with the non-destructive
# one - the stock SystemInit() resets RCC and would kill the OSPI clock.
# ---------------------------------------------------------------------------
set(H723_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app/app.ld" CACHE FILEPATH
    "QSPI app linker script (code at 0x90000000)" FORCE)
set(H723_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/app/src/system_app.c" CACHE FILEPATH
    "QSPI app system init (non-destructive)" FORCE)

add_executable(app.elf
    app/src/main.c
    app/src/system_app.c
)
stm32h723_apply_board(app.elf "-O2")
target_compile_definitions(app.elf PRIVATE QSPI_APP=1)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/app.hex
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:app.elf> ${CMAKE_CURRENT_BINARY_DIR}/app.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:app.elf>
    DEPENDS app.elf
    COMMENT "objcopy -> app.hex (Intel HEX @ 0x90000000)")
add_custom_target(app_hex ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/app.hex)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/app.bin
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:app.elf> ${CMAKE_CURRENT_BINARY_DIR}/app.bin
    DEPENDS app.elf
    COMMENT "objcopy -> app.bin (raw, for bootloader UART download / ext flash)")
add_custom_target(app_bin DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/app.bin)

# ---------------------------------------------------------------------------
# Flashing
# ---------------------------------------------------------------------------
# DEBUG_PROBE is optional: empty (default) makes probe-rs auto-detect the
# connected probe. To pin a specific probe, view its selector with `probe-rs
# list` (e.g. 0483:374b:xxxx... for an ST-Link V2) and pass -DDEBUG_PROBE=...
set(DEBUG_PROBE "" CACHE STRING
    "probe-rs --probe selector (VID:PID[:Serial], e.g. 0483:374b:xxxx...); empty = auto-detect")

if(DEBUG_PROBE)
    set(PROBE_ARGS --probe "${DEBUG_PROBE}")
else()
    set(PROBE_ARGS)
endif()

find_program(PROBE_RS NAMES probe-rs probe-rs.exe
    HINTS "$ENV{USERPROFILE}/.cargo/bin" "$ENV{CARGO_HOME}/bin")

if(PROBE_RS)
    add_custom_target(flash_boot
        COMMAND "${PROBE_RS}" download ${PROBE_ARGS}
                    --chip "STM32H723ZG" --protocol swd
                    --connect-under-reset
                    --binary-format hex --verify --reset --non-interactive
                    --disable-progressbars ${CMAKE_CURRENT_BINARY_DIR}/boot.hex
        DEPENDS boot_hex
        COMMENT "Flashing boot.hex to STM32H723ZG via probe-rs (ST-Link V2, SWD) ..."
        USES_TERMINAL)
    add_custom_target(flash DEPENDS flash_boot)
else()
    message(WARNING "probe-rs not found - 'ninja flash' will not work")
endif()
