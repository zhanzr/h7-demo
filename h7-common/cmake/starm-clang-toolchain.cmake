# CMake toolchain file for ST's Arm Clang (starm-clang) + LLD.
#
# starm-clang is STMicroelectronics' LLVM/Clang toolchain shipped with
# STM32CubeIDE (the `llvm.win32_*` plugin, e.g.
# D:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE/plugins/.../tools). Its driver already
# targets arm-st-none-eabi, selects the matching newlib multilib from its own
# sysroot, and links with LLVM's LLD.
#
# C compilation: starm-clang (LLVM)
# Assembly:      GNU arm-none-eabi-gcc (the ST startup file uses GAS syntax)
# Linking:       starm-clang driver -> ld.lld + the bundled newlib
#
# Selection (project CMakeLists):
#   set(STM32_TOOLCHAIN "starm-clang" CACHE STRING "...")
# or from the command line:
#   cmake -G Ninja -DSTM32_TOOLCHAIN=starm-clang ..
#
# Variables:
#   STARM_ROOT   : ST Arm Clang tools root (contains bin/starm-clang.exe)
#   ARM_GCC_ROOT : GNU arm-none-eabi root (assembler only, for the .s startup)

cmake_minimum_required(VERSION 3.13)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m7)

# CMake 3.30+ ARMClang support: we pass cpu/arch flags ourselves.
if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

set(STARM_ROOT "D:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.llvm.win32_1.0.200.202603311046/tools" CACHE PATH
    "ST Arm Clang tools root (contains bin/starm-clang.exe)")
set(ARM_GCC_ROOT "D:/Arm/GNU Toolchain mingw-w64-x86_64-arm-none-eabi" CACHE PATH
    "GNU arm-none-eabi root (assembler for the GAS startup file)")

# find_program paths must be in the flavour the running cmake understands: MSYS
# cmake splits a Windows-style "D:/..." hint at the colon (it treats it as a
# PATH list) and re-roots the leading "D" against the build dir. h7_tool_path
# converts to "/d/..." for MSYS cmake and keeps "d:/..." elsewhere.
include(${CMAKE_CURRENT_LIST_DIR}/tool-path.cmake)
h7_tool_path("${STARM_ROOT}/bin" _STARM_BIN)
h7_tool_path("${ARM_GCC_ROOT}/bin" _GNU_BIN)

find_program(CMAKE_C_COMPILER NAMES starm-clang starm-clang.exe
    HINTS "${_STARM_BIN}" REQUIRED)
find_program(CMAKE_ASM_COMPILER NAMES arm-none-eabi-gcc arm-none-eabi-gcc.exe
    HINTS "${_GNU_BIN}" REQUIRED)
find_program(CMAKE_OBJCOPY NAMES starm-objcopy starm-objcopy.exe
    HINTS "${_STARM_BIN}" REQUIRED)
find_program(CMAKE_OBJDUMP NAMES starm-objdump starm-objdump.exe
    HINTS "${_STARM_BIN}" REQUIRED)
find_program(CMAKE_SIZE NAMES starm-size starm-size.exe
    HINTS "${_STARM_BIN}" REQUIRED)
find_program(CMAKE_AR NAMES starm-ar starm-ar.exe
    HINTS "${_STARM_BIN}" REQUIRED)

# Bare-metal target: compiler sanity check links against a static library.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Targets are named *.elf explicitly; do not let CMake add a suffix.
set(CMAKE_EXECUTABLE_SUFFIX "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Tells cmake/stm32h723_board.cmake to apply starm-clang-specific tweaks
# (LLD link line with the M7 multilib, clang-compatible warning flags).
set(STM32_STARM_CLANG TRUE)
