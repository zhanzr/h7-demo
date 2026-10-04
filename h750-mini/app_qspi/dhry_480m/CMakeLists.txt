# Dhrystone 2.1 @ 480 MHz â€?pure-QSPI variant (dhry_480m)
#
# Identical benchmark sources + flags to dhry_480m, but the code is linked at
# 0x90000000 and executes out of the on-board W25Q64 (pure-QSPI code space),
# booted by h750_boot. See ../QSPI_APP_GUIDE.md for the QSPI-app rules.
#
# Targets:
#   ninja         - builds dhry_480m.elf + .hex (@ 0x90000000) + .bin
#   ninja bin     - raw .bin (optional)
#   ninja flash   - flashes the W25Q64 via the probe-rs QUADSPI algorithm
#                   (auto-builds the algorithm YAML if missing/stale). Note this
#                   programs the EXTERNAL flash; h750_boot must be in internal
#                   flash first. See README.md / QSPI_APP_GUIDE.md.

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc (arm-none-eabi, default), armclang (Keil AC6) or starm-clang (ST Arm Clang)")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang starm-clang)
option(STM32_LTO "Enable -flto with the GCC toolchain" OFF)

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(STM32_TOOLCHAIN STREQUAL "armclang")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-keil-toolchain.cmake")
    elseif(STM32_TOOLCHAIN STREQUAL "starm-clang")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/starm-clang-toolchain.cmake")
    else()
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
    endif()
endif()

project(dhry_480m C ASM)

# armclang only: restore the GNU-style link rule after project() (CMake's
# ARMClang module installs an armlink-style rule that GNU ld cannot use).
if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h750_board.cmake)

# QSPI app: link at 0x90000000 and use the non-destructive SystemInit (the
# bootloader owns the 480 MHz clock tree / PLL2 -> QUADSPI clock).
set(H750_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app.ld" CACHE FILEPATH
    "QSPI app linker script (code at 0x90000000)" FORCE)
set(H750_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/system_app.c" CACHE FILEPATH
    "QSPI app system init (non-destructive)" FORCE)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/system_app.c
    src/utils.c
    src/dhry_1.c
    src/dhry_2.c
)
# Benchmark flags, mirroring the h723-mini benchmark projects so both
# boards' results are directly comparable:
#   BENCH_OPT   - every translation unit (compile + link)
#   BENCH_OPT_C - C only (e.g. armclang -Omax, which it rejects for ASM)
set(BENCH_OPT "-Ofast -ffp-contract=fast -funroll-loops" CACHE STRING "Optimisation flags for the whole build")
set(BENCH_OPT_C "" CACHE STRING "Extra C-only optimisation flags (e.g. armclang -Omax -fno-lto)")
stm32h750_apply_board(${PROJECT_NAME}.elf "${BENCH_OPT} ${BENCH_OPT_C}")

# Report the flags actually used: CoreMark prints COMPILER_FLAGS (FLAGS_STR),
# so the console line matches the build instead of a hardcoded string.
set(BENCH_FLAGS_STR "${BENCH_OPT}")
if(BENCH_OPT_C)
    string(APPEND BENCH_FLAGS_STR " ${BENCH_OPT_C}")
endif()
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE "FLAGS_STR=\"${BENCH_FLAGS_STR}\"")

# QSPI app: make board.c's SystemClock_Config()/MPU a no-op so the identical
# main() can call Board_Init() without breaking the bootloader's clocks/MPU.
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE QSPI_APP=1)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)
