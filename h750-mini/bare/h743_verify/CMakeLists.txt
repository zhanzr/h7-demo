# h743_verify: probe whether the "STM32H750VBT6" really has 2 Mbyte flash.
# Builds a firmware image deliberately > 128 KB (512 KB probe array pinned to
# 0x08030000 by h743_verify.ld, which declares FLASH = 2M). Flashing it with
# the H743 chip definition (CHIP=STM32H743VI) tests the rumor on hardware.
#
# Workflow:
#   bash build.sh                           # default: GNU arm-none-eabi-gcc
#   ninja           # builds .elf + .hex (hex is what `ninja flash` uses)
#   ninja flash     # flashes .hex with CHIP=STM32H743VI (2 MB def) - this
#                   # project sets PROBE_RS_CHIP=STM32H743VI automatically
#   ninja bin       # optional raw .bin image, "in case" you need it

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

# Toolchain selection. gcc (default) = GNU arm-none-eabi end-to-end;
# armclang = Keil Arm Compiler 6 C + GNU as/ld. CMAKE_TOOLCHAIN_FILE is
# cached, so use a separate build dir per toolchain (build/ for gcc,
# build-ac6/ for armclang).
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

project(h743_verify C ASM)

if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
)

# This project must link against 2 Mbyte of flash (h743_verify.ld) so the
# >128 KB probe array fits. Override the shared board script via the cache
# variable BEFORE including the board layer (which only fills the cache var if
# it is unset).
set(H750_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.ld" CACHE FILEPATH
    "h743_verify linker script (FLASH = 2M)")

# `ninja flash` must use the 2 Mbyte chip definition or probe-rs refuses the
# >128 KB image (a "No flash memory contains ... 0x08020000" error). Force the
# cache var (FORCE) so it cannot be silently left at the H750 default.
set(PROBE_RS_CHIP "STM32H743VI" CACHE STRING
    "probe-rs target chip for the 2 MB-flash verification" FORCE)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h750_board.cmake)
stm32h750_apply_board(${PROJECT_NAME}.elf "-O2")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
