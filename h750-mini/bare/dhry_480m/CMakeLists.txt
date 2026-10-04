# Dhrystone 2.1 benchmark for the STM32H750VBTx (h750-mini) @ 480 MHz.
# GCC (default) at the most aggressive optimization (-Ofast -ffp-contract=fast
# -funroll-loops) + hard-float, optionally with -flto (STM32_LTO=ON); Keil Arm
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
#   ninja flash     # programs the board (probe-rs, ULINK2 CMSIS-DAP / SWD)
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

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/utils.c
    src/dhry_1.c
    src/dhry_2.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h750_board.cmake)
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

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
