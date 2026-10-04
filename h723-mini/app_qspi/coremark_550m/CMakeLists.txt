# coremark_550m (app_qspi): CoreMark 1.0 from the W25Q64 (OCTOSPI memory-mapped).
# Same sources as bare/coremark_550m, linked at 0x90000000 and booted by
# h723_boot.
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

project(coremark_550m C ASM)

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

set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/coremark_1_0_1)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/system_app.c
    src/utils.c
    src/core_portme.c
    ${CORE}/core_list_join.c
    ${CORE}/core_main.c
    ${CORE}/core_matrix.c
    ${CORE}/core_state.c
    ${CORE}/core_util.c
)

# Benchmark optimization flags (interface borrowed from the nano-f411 "f4-demo"
# benchmarks, see README.md): -funroll-all-loops measured ~5% faster than
# -funroll-loops on CoreMark there.
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

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE QSPI_APP=1)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)
