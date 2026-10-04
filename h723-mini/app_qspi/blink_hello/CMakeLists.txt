# blink_hello (app_qspi): minimal STM32H723ZGT6 (h723-mini) QSPI app @ 550 MHz.
# Same as bare/blink_hello but linked for and booted from the W25Q64 at
# 0x90000000 (OCTOSPI memory-mapped), by h723_boot in internal flash.
#
# Pico-style workflow:
#   bash build.sh                           # default: GNU arm-none-eabi-gcc
#   ninja           # builds .elf + .hex (hex is what `ninja flash` uses)
#   ninja flash     # writes the W25Q64 via the OCTOSPI flash algorithm
#   ninja bin       # optional raw .bin image

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

project(blink_hello C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h723_board.cmake)

# QSPI app: link at 0x90000000 and use the non-destructive SystemInit (the
# bootloader owns the 550 MHz clock tree / OSPI clock).
set(H723_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app.ld" CACHE FILEPATH
    "QSPI app linker script (code at 0x90000000)" FORCE)
set(H723_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/system_app.c" CACHE FILEPATH
    "QSPI app system init (non-destructive)" FORCE)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/system_app.c
)

stm32h723_apply_board(${PROJECT_NAME}.elf "-O2")

# QSPI app: make board.c's SystemClock_Config()/MPU a no-op so the identical
# main() can call Board_Init() without breaking the bootloader's clocks/MPU.
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE QSPI_APP=1)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)
