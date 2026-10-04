# CoreMark 1.0.1 benchmark for the STM32H723ZGT6 (h723-mini) @ 550 MHz.
# GCC (default) at the most aggressive optimization (-Ofast -ffp-contract=fast
# -funroll-all-loops) + hard-float, optionally with -flto (STM32_LTO=ON); Keil Arm
# Compiler 6 (armclang) is selectable via -DSTM32_TOOLCHAIN=armclang.
# Linked with GNU ld/newlib via the existing linker script.
#
# Pico-style workflow:
#   bash build.sh                           # default: GNU arm-none-eabi-gcc
#   mkdir -p build-gcc && cd build-gcc      # optional: gcc + LTO
#   cmake -G Ninja -DSTM32_LTO=ON ..
#   mkdir -p build-ac6 && cd build-ac6      # optional: Keil AC6 (armclang)
#   cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..
#   ninja           # builds .elf + .hex (hex is what `ninja flash` uses)
#   ninja flash     # programs the board (probe-rs, ST-Link V2 / SWD)
#   ninja bin       # optional raw .bin image, "in case" you need it

cmake_minimum_required(VERSION 3.13)

# We pass the cpu/arch flags ourselves (-mcpu=...) and link with GNU ld, so
# keep CMake's ARMClang module from auto-adding --cpu/--march flags.
if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

# Toolchain selection. gcc (default) = GNU arm-none-eabi end-to-end;
# armclang = Keil Arm Compiler 6 C + GNU as/ld. CMAKE_TOOLCHAIN_FILE is
# cached, so use a separate build dir per toolchain (build/ for gcc,
# build-gcc-lto/ for gcc+LTO, build-ac6/ for armclang).
set(STM32_TOOLCHAIN "gcc" CACHE STRING "C compiler: gcc (arm-none-eabi, default), armclang (Keil AC6) or starm-clang (ST Arm Clang)")
set_property(CACHE STM32_TOOLCHAIN PROPERTY STRINGS gcc armclang starm-clang)

# Link-time optimization. GCC only: armclang -flto/-Omax emits LLVM bitcode
# (.llvm.lto) that needs armlink + ARMCLIB and cannot be consumed by GNU ld.
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

project(coremark_550m C ASM)

# armclang only: restore the GNU-style link rule after project() (CMake's
# ARMClang module installs an armlink-style rule that GNU ld cannot use).
if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/coremark_1_0_1)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/utils.c
    src/core_portme.c
    ${CORE}/core_list_join.c
    ${CORE}/core_main.c
    ${CORE}/core_matrix.c
    ${CORE}/core_state.c
    ${CORE}/core_util.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h723_board.cmake)

# Benchmark optimization flags (interface borrowed from the nano-f411 "f4-demo"
# benchmarks, see README.md): GCC at the most aggressive setting, with
# -funroll-all-loops, which measured ~5% faster than -funroll-loops on CoreMark
# there (282.35 vs 267.78 it/s) and ~6.6% with LTO.
set(BENCH_OPT "-Ofast -ffp-contract=fast -funroll-all-loops" CACHE STRING
    "Optimization flags for stm32h723_apply_board (e.g. -Omax for armclang)")
set(BENCH_OPT_C "" CACHE STRING
    "Extra C-language-only flags (e.g. armclang '-Omax -fno-lto'; not passed to ASM/link)")
stm32h723_apply_board(${PROJECT_NAME}.elf "${BENCH_OPT}")
if(BENCH_OPT_C)
    separate_arguments(BENCH_OPT_C_LIST NATIVE_COMMAND "${BENCH_OPT_C}")
    target_compile_options(${PROJECT_NAME}.elf PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:${BENCH_OPT_C_LIST}>")
endif()

# Report the flags actually used: CoreMark prints COMPILER_FLAGS (FLAGS_STR),
# so the console line matches the build instead of a hardcoded string.
set(BENCH_FLAGS_STR "${BENCH_OPT}")
if(BENCH_OPT_C)
    string(APPEND BENCH_FLAGS_STR " ${BENCH_OPT_C}")
endif()
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE "FLAGS_STR=\"${BENCH_FLAGS_STR}\"")

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    src
    ${CORE}
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
