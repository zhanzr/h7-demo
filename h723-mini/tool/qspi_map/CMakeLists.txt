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
#   ninja flash_boot - probe-rs -> boot.hex into internal flash (STM32H723ZG),
#                      probe auto-detected
#   ninja flash_boot-stlink|-dap|-jlink|-ulink - same, forcing that probe family
#   ninja flash      - alias for flash_boot (and flash-stlink / -dap / -jlink too)
#   ninja probes     - list the probes probe-rs can see right now
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
    ../../bare/spi_flash_test/src/w25q64.c
)
stm32h723_apply_board(boot.elf "-O2")
target_sources(boot.elf PRIVATE
    ${H723_HAL}/Src/stm32h7xx_hal_ospi.c
    ${H723_HAL}/Src/stm32h7xx_hal_mdma.c)
target_include_directories(boot.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../bare/spi_flash_test/src)

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
# Flashing: ninja flash_boot / flash_boot-<probe-family>, plus `flash` aliases
# ---------------------------------------------------------------------------
# The probe families are resolved from `probe-rs list` at configure time by
# cmake/probe-select.cmake. Overrides:
#   -DDEBUG_PROBE=VID:PID[:SERIAL]    pin one probe for every flash target
#   PROBE_RS_PROBE=VID:PID[:SERIAL]   same, for a single run
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/probe-select.cmake)

if(PROBE_RS)
    # No --connect-under-reset: on this ST-Link V2 with probe-rs 0.32 the
    # under-reset attach never asserts nRST (RCC_RSR shows no PINRSTF after it),
    # so the chip is not reset and probe-rs times out waiting for core 0 to halt
    # ("Timeout while attaching to target under reset"). A plain attach halts the
    # core, which is all an internal-flash download needs.
    #
    # One target per probe family; the command line is spelled out at each
    # add_custom_target() call so CMake keeps every argument intact.
    foreach(_fam auto stlink dap jlink)
        if(_fam STREQUAL "auto")
            set(_tgt flash_boot)
        else()
            set(_tgt flash_boot-${_fam})
        endif()

        h723_probe_ready(${_fam} _ready)
        if(NOT _ready)
            h723_probe_hint(${_fam} _hint)
            add_custom_target(${_tgt}
                COMMAND ${CMAKE_COMMAND} -E echo "${_hint}"
                COMMAND ${CMAKE_COMMAND} -E false
                COMMENT "${_tgt}: no ${_fam} probe")
            continue()
        endif()

        h723_probe_args(${_fam} _pargs)
        if(_pargs)
            set(_how "${_fam} probe")
        else()
            set(_how "probe-rs auto-detect")
        endif()

        add_custom_target(${_tgt}
            COMMAND "${PROBE_RS}" download ${_pargs}
                        --chip "STM32H723ZG" --protocol swd
                        --binary-format hex --verify --reset --non-interactive
                        --disable-progressbars "${CMAKE_CURRENT_BINARY_DIR}/boot.hex"
            DEPENDS boot_hex
            COMMENT "Flashing boot.hex to STM32H723ZG via probe-rs (SWD) [${_how}]"
            USES_TERMINAL)
    endforeach()

    h723_add_ulink_stub(flash_boot)

    # `ninja flash` keeps working as before, and mirrors every probe variant.
    add_custom_target(flash DEPENDS flash_boot)
    foreach(_fam stlink dap jlink ulink)
        add_custom_target(flash-${_fam} DEPENDS flash_boot-${_fam})
    endforeach()
else()
    message(WARNING "probe-rs not found - 'ninja flash' will not work")
endif()
