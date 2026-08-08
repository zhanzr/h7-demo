# probers_alg: run the flash algorithm's register-level OCTOSPI code as a
# normal, debuggable firmware (the "host" that simulates probe-rs loading the
# algorithm). Starts on HSI (replicating the algorithm context) and prints the
# JEDEC read result + program/erase/read-back cycle over USART1 for real
# visibility.
#
# Targets:
#   ninja        - builds probers_alg.elf + probers_alg.hex (internal flash)
#   ninja flash  - probe-rs -> internal flash (STM32H723ZG)

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

project(probers_alg C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h723_board.cmake)

# The board layer's stock system init is used, but main() deliberately does NOT
# call the HSE-based SystemClock_Config() - it runs on the reset-default HSI.

add_executable(${PROJECT_NAME}.elf
    src/main.c
)
stm32h723_apply_board(${PROJECT_NAME}.elf "-O2")

# ALGO_DEBUG turns on printf dumps inside the algorithm's register-level OSPI
# write path (only present in this harness, never in the real flash algorithm).
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE ALGO_DEBUG=1)

# For the HAL-based cross-check (does the OSPI work at HSI 64 MHz?).
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${H723_HAL}/Src/stm32h7xx_hal_ospi.c
    ${H723_HAL}/Src/stm32h7xx_hal_mdma.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
