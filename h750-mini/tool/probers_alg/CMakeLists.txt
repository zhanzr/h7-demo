# probers_alg: run the flash algorithm's register-level QUADSPI code as a
# normal, debuggable firmware (the "host" that simulates probe-rs loading the
# algorithm). Starts on HSI (replicating the algorithm context) and prints every
# clock/QSPI register + the JEDEC read result over USART1 for real visibility.
#
# Targets:
#   ninja        - builds probers_alg.elf + probers_alg.hex (internal flash)
#   ninja flash  - probe-rs -> internal flash (STM32H750VB)
#
# Targets:
#   ninja        - builds probers_alg.elf + probers_alg.hex (internal flash)
#   ninja flash  - probe-rs -> internal flash (STM32H750VB)

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

project(probers_alg C ASM)

# armclang only: restore the GNU-style link rule after project() (CMake's
# ARMClang module installs an armlink-style rule that GNU ld cannot use).
if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h750_board.cmake)

# The board layer's stock system init is used, but main() deliberately does NOT
# call the HSE-based SystemClock_Config() - it runs on the reset-default HSI.

add_executable(${PROJECT_NAME}.elf
    src/main.c
)
stm32h750_apply_board(${PROJECT_NAME}.elf "-O2")

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
