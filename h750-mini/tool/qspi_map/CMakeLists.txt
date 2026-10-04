# qspi_map: two-stage firmware that boots the STM32H750 from internal flash and
# runs the main app from the QSPI-mapped space (0x90000000).
#
#   boot.elf  - stage 1 bootloader in internal flash (128 KB; a 2 MB variant is
#               available via -DBOOT_FLASH_2M=ON for the shadow-H743 part).
#               Initializes clocks/UART/QSPI, optionally downloads the app over
#               UART, then jumps to 0x90000000.
#   app.elf   - stage 2 app, linked at 0x90000000 (QSPI memory-mapped). Runs
#               entirely from the W25Q64. blink_hello-like behavior.
#
# Targets:
#   ninja            - builds both .elf + .hex
#   ninja flash_boot - probe-rs -> boot.hex into internal flash (STM32H750VB)
#   ninja flash      - alias for flash_boot
#   ninja bin        - app.raw .bin (for the bootloader UART download / ext flash)
#
# The stage-2 app is flashed into the W25Q64 separately:
#   * bootloader UART download mode (send app.bin), or
#   * openocd stmqspi (tool/openocd_qspi_h750.cfg), or
#   * probe-rs custom W25Q64 flash algorithm (see README).
# See README.md for the flashing flow and benchmarks.

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc (arm-none-eabi, default) or armclang (Keil AC6)")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang)
option(STM32_LTO "Enable -flto with the GCC toolchain" OFF)
option(BOOT_FLASH_2M "Build the bootloader against 2 Mbyte internal flash (shadow STM32H743VI)" OFF)

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(STM32_TOOLCHAIN STREQUAL "armclang")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-keil-toolchain.cmake")
    else()
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
    endif()
endif()

project(qspi_map C ASM)

# armclang only: restore the GNU-style link rule after project() (CMake's
# ARMClang module installs an armlink-style rule that GNU ld cannot use).
if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h750_board.cmake)

# ---------------------------------------------------------------------------
# Stage 1: bootloader (internal flash)
# ---------------------------------------------------------------------------
# NOTE: H750_LINKER_SCRIPT / H750_SYSTEM_SOURCE are SHARED cache vars read by
# stm32h750_apply_board(). FORCE is required before each target, otherwise the
# value left over from the previous target / configure run wins.
if(BOOT_FLASH_2M)
    set(H750_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/boot/boot_2m.ld" CACHE FILEPATH
        "bootloader linker script (2M shadow-H743) " FORCE)
else()
    set(H750_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/boot/boot.ld" CACHE FILEPATH
        "bootloader linker script (128K internal flash)" FORCE)
endif()
set(H750_SYSTEM_SOURCE "${BOARD_DIR}/system_stm32h7xx.c" CACHE FILEPATH
    "bootloader system init (stock, owns the clocks)" FORCE)

add_executable(boot.elf
    boot/src/main.c
    boot/src/app_image.c        # generated from app.bin (regenerate after app changes)
    ../../bare/spi_flash_test/src/w25q64.c
)
stm32h750_apply_board(boot.elf "-O2")
target_sources(boot.elf PRIVATE ${H750_HAL}/Src/stm32h7xx_hal_qspi.c)
target_include_directories(boot.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../bare/spi_flash_test/src)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/boot.hex
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:boot.elf> ${CMAKE_CURRENT_BINARY_DIR}/boot.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:boot.elf>
    DEPENDS boot.elf
    COMMENT "objcopy -> boot.hex (Intel HEX)")
add_custom_target(boot_hex ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/boot.hex)

# ---------------------------------------------------------------------------
# Stage 2: QSPI app (0x90000000). Override system source with the non-destructive
# one - the stock SystemInit() resets RCC and would kill the QSPI clock.
# ---------------------------------------------------------------------------
set(H750_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app/app.ld" CACHE FILEPATH
    "QSPI app linker script (code at 0x90000000)" FORCE)
set(H750_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/app/src/system_app.c" CACHE FILEPATH
    "QSPI app system init (non-destructive)" FORCE)

add_executable(app.elf
    app/src/main.c
    app/src/system_app.c
)
stm32h750_apply_board(app.elf "-O2")

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
set(ULINK2_PROBE "c251:2722:V0010M9E" CACHE STRING "probe-rs --probe selector (ULINK2 CMSIS-DAP)")
set(PROBE_RS_CHIP "STM32H750VB" CACHE STRING "probe-rs target chip (STM32H743VI for the 2M boot)")

find_program(PROBE_RS NAMES probe-rs probe-rs.exe
    HINTS "$ENV{USERPROFILE}/.cargo/bin" "$ENV{CARGO_HOME}/bin")

if(PROBE_RS)
    add_custom_target(flash_boot
        COMMAND "${PROBE_RS}" download --probe "${ULINK2_PROBE}"
                    --chip "${PROBE_RS_CHIP}" --protocol swd
                    --binary-format hex --verify --reset --non-interactive
                    --disable-progressbars ${CMAKE_CURRENT_BINARY_DIR}/boot.hex
        DEPENDS boot_hex
        COMMENT "Flashing boot.hex to ${PROBE_RS_CHIP} via probe-rs (ULINK2 CMSIS-DAP, SWD) ..."
        USES_TERMINAL)
    add_custom_target(flash DEPENDS flash_boot)
else()
    message(WARNING "probe-rs not found - 'ninja flash' will not work")
endif()
