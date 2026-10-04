# dhry_550m (app_qspi): Dhrystone 2.1 from the W25Q64 (OCTOSPI memory-mapped).
# Same sources as bare/dhry_550m, linked at 0x90000000 and booted by h723_boot.
#
# Pico-style workflow:
#   bash build.sh
#   ninja           # builds .elf + .hex
#   ninja flash     # writes the W25Q64 via the OCTOSPI flash algorithm

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc (arm-none-eabi, default), armclang (Keil AC6) or starm-clang (ST Arm Clang)")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang starm-clang)
option(STM32_LTO "Enable LTO (gcc or starm-clang only)" OFF)

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(STM32_TOOLCHAIN STREQUAL "armclang")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-keil-toolchain.cmake")
    elseif(STM32_TOOLCHAIN STREQUAL "starm-clang")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/starm-clang-toolchain.cmake")
    else()
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
    endif()
endif()

project(dhry_550m C ASM)

# armclang only: restore the GNU-style link rule after project() (CMake's
# ARMClang module installs an armlink-style rule that GNU ld cannot use).
if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h723_board.cmake)

set(H723_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/app.ld" CACHE FILEPATH
    "QSPI app linker script (code at 0x90000000)" FORCE)
set(H723_SYSTEM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/system_app.c" CACHE FILEPATH
    "QSPI app system init (non-destructive)" FORCE)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/system_app.c
    src/utils.c
    src/dhry_1.c
    src/dhry_2.c
)

# Benchmark optimization flags (interface borrowed from the nano-f411 "f4-demo"
# benchmarks, see README.md). Dhrystone keeps -funroll-loops and must NOT use
# -flto (it hoists work out of the timed loop and inflates the score).
set(BENCH_OPT "-Ofast -ffp-contract=fast -funroll-loops" CACHE STRING
    "Optimization flags for stm32h723_apply_board (e.g. -Omax for armclang)")
set(BENCH_OPT_C "" CACHE STRING
    "Extra C-language-only flags (e.g. armclang '-Omax -fno-lto'; not passed to ASM/link)")
stm32h723_apply_board(${PROJECT_NAME}.elf "${BENCH_OPT}")
if(BENCH_OPT_C)
    separate_arguments(BENCH_OPT_C_LIST NATIVE_COMMAND "${BENCH_OPT_C}")
    target_compile_options(${PROJECT_NAME}.elf PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:${BENCH_OPT_C_LIST}>")
endif()

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE QSPI_APP=1)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)
