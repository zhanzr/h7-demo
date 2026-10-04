# spi_flash_test: W25Q64 (8 MB) OSPI flash benchmark on the h723-mini.
#
# Measures read/write/erase throughput in every W25Q64 line mode over the
# console, and demonstrates XIP (executing a position-independent function from
# the OSPI memory-mapped space at 0x90000000). Uses the shared board layer
# (550 MHz, USART1 console) plus the HAL OSPI driver.
#
# Workflow:
#   bash build.sh      # configure + build with GNU arm-none-eabi-gcc
#   ninja              # rebuilds .elf + .hex
#   ninja flash        # flash internal 1 MB app via probe-rs (STM32H723ZG)
#   ninja bin          # optional raw .bin
#
# See README.md for expected numbers and the vendor-driver comparison.

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

# Toolchain selection. gcc (default) = GNU arm-none-eabi end-to-end;
# armclang = Keil Arm Compiler 6 C + GNU as/ld. Use a separate build dir per
# toolchain (build/ for gcc, build-ac6/ for armclang).
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

project(spi_flash_test C ASM)

if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/w25q64.c
    src/qspi_xip.S          # position-independent XIP demo fn (.qspi_code)
)

# This project uses its own linker script (adds the .qspi_code section; the
# internal flash layout itself is unchanged, 1 MB). Override the shared board
# script via the cache variable BEFORE including the board layer.
set(H723_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.ld" CACHE FILEPATH
    "spi_flash_test linker script (1M internal + .qspi_code)")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h723_board.cmake)
stm32h723_apply_board(${PROJECT_NAME}.elf "-O2")

# OSPI HAL driver (not part of the generic board source list) + MDMA (the OSPI
# HAL's HAL_OSPI_Abort references HAL_MDMA_Abort).
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${H723_HAL}/Src/stm32h7xx_hal_ospi.c
    ${H723_HAL}/Src/stm32h7xx_hal_mdma.c)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
