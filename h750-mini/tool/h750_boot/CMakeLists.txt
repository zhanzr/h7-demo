# h750_boot: minimal STM32H750 (h750-mini) bootloader in internal flash.
#
# Prints a banner, checks the W25Q64 (QSPI) for a bootable stage-2 firmware at
# the QSPI memory-mapped base 0x90000000 (initial stack top in DTCM + a thumb
# reset vector inside the QSPI window), and jumps to it. If the check fails it
# loops, toggling PA8 and printing the check result.
#
# The stage-2 firmware is written to the W25Q64 separately (probe-rs custom
# flash algorithm, openocd, or the bootloader-UART loader) - see README.md.
#
# Targets:
#   ninja        - builds h750_boot.elf + h750_boot.hex (internal flash)
#   ninja flash  - probe-rs -> h750_boot.hex into internal flash (STM32H750VB)

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc (arm-none-eabi, default) or armclang (Keil AC6)")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang)
option(STM32_LTO "Enable -flto with the GCC toolchain" OFF)

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(STM32_TOOLCHAIN STREQUAL "armclang")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-keil-toolchain.cmake")
    else()
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
    endif()
endif()

project(h750_boot C ASM)

# armclang only: restore the GNU-style link rule after project() (CMake's
# ARMClang module installs an armlink-style rule that GNU ld cannot use).
if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h750_board.cmake)

# The bootloader lives in (and owns) internal flash + the 480 MHz clock tree,
# so the default board linker script (stm32h750vbt.ld) and the stock system
# init (system_stm32h7xx.c) are exactly what apply_board() already uses.

add_executable(${PROJECT_NAME}.elf
    src/main.c
    ../../bare/spi_flash_test/src/w25q64.c
)
stm32h750_apply_board(${PROJECT_NAME}.elf "-O2")
target_sources(${PROJECT_NAME}.elf PRIVATE ${H750_HAL}/Src/stm32h7xx_hal_qspi.c)
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../bare/spi_flash_test/src)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
