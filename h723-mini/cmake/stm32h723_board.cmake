# Shared STM32H723ZG (h723-mini) board layer for the benchmark projects.
# The board support (clock init to 550 MHz core / 275 MHz HCLK from the 25 MHz
# HSE, USART1 console, SWV/ITM enable, newlib stubs, startup, linker script)
# plus the STM32H7 HAL driver sources are attached to a target with
# stm32h723_apply_board().
#
# Usage (from a project CMakeLists.txt, after add_executable()):
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/stm32h723_board.cmake)
#   stm32h723_apply_board(${PROJECT_NAME}.elf "-Ofast")
#
# Requires the project to enable ASM (project(X C ASM)).

set(H723_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH
    "Root of the h723-mini board tree (contains board/ and cmake/)")

set(BOARD_DIR ${H723_ROOT}/board)
# Shared H7 chip-level drivers (HAL + CMSIS) live in the repo-level h7-common/
# tree, shared by every board; only board-level support stays under h723-mini/.
set(H723_DRV "${H723_ROOT}/../h7-common/drivers")
set(H723_HAL ${H723_DRV}/STM32H7xx_HAL_Driver)
set(H723_CMSIS ${H723_DRV}/CMSIS)
set(H723_CMSDEV ${H723_CMSIS}/Device/ST/STM32H7xx)

# Linker script for the STM32H723ZG build (1 MB flash, DTCM RW/ZI).
set(H723_LINKER_SCRIPT "${BOARD_DIR}/stm32h723zg.ld" CACHE FILEPATH
    "Linker script for the STM32H723 build")

# System init source. Defaults to the board copy of system_stm32h7xx.c (resets
# the RCC clock tree at startup - correct for a firmware that owns the clocks).
set(H723_SYSTEM_SOURCE "${BOARD_DIR}/system_stm32h7xx.c" CACHE FILEPATH
    "System init source for the STM32H723 build")

# h7_tool_path(): keeps tool/lib paths in the flavour the shell that cmake
# generates for the build understands (MSYS cmake -> sh, mingw cmake -> cmd).
include(${CMAKE_CURRENT_LIST_DIR}/tool-path.cmake)

function(stm32h723_apply_board TGT OPT)
    separate_arguments(OPT_LIST NATIVE_COMMAND "${OPT}")

    # GCC-only warning switches; keep the clang-based toolchains clean.
    if(STM32_ARMCLANG)
        set(_WARN_FLAGS -Wall)
    elseif(STM32_STARM_CLANG)
        set(_WARN_FLAGS -Wall -Wno-unused-command-line-argument)
    else()
        set(_WARN_FLAGS
            -Wall
            -Wno-unused-but-set-variable -Wno-unused-function
            -Wno-unused-variable -Wno-unused-parameter -Wno-maybe-uninitialized)
    endif()

    target_sources(${TGT} PRIVATE
        ${BOARD_DIR}/board.c
        ${BOARD_DIR}/swv_printf.c
        ${BOARD_DIR}/uart_printf.c
        ${BOARD_DIR}/syscalls.c
        ${BOARD_DIR}/startup_stm32h723xx.s
        ${H723_SYSTEM_SOURCE}
        ${H723_HAL}/Src/stm32h7xx_hal.c
        ${H723_HAL}/Src/stm32h7xx_hal_cortex.c
        ${H723_HAL}/Src/stm32h7xx_hal_flash.c
        ${H723_HAL}/Src/stm32h7xx_hal_flash_ex.c
        ${H723_HAL}/Src/stm32h7xx_hal_gpio.c
        ${H723_HAL}/Src/stm32h7xx_hal_hsem.c
        ${H723_HAL}/Src/stm32h7xx_hal_pwr.c
        ${H723_HAL}/Src/stm32h7xx_hal_pwr_ex.c
        ${H723_HAL}/Src/stm32h7xx_hal_rcc.c
        ${H723_HAL}/Src/stm32h7xx_hal_rcc_ex.c
        ${H723_HAL}/Src/stm32h7xx_hal_uart.c
        ${H723_HAL}/Src/stm32h7xx_hal_uart_ex.c
        ${H723_HAL}/Src/stm32h7xx_ll_delayblock.c
    )

    target_include_directories(${TGT} PRIVATE
        ${BOARD_DIR}
        ${H723_HAL}/Inc
        ${H723_HAL}/Inc/Legacy
        ${H723_CMSDEV}/Include
        ${H723_CMSIS}/Include
    )

    # armclang has no bundled libc headers: point it at the GNU newlib include
    # dir so <stdio.h>/<string.h>/... resolve to the same newlib we link.
    # h7_tool_path keeps the path in the flavour the generated shell needs.
    if(STM32_ARMCLANG)
        h7_tool_path("${ARM_GCC_ROOT}/arm-none-eabi/include" _ARMCLANG_NEWLIB_INC)
        target_include_directories(${TGT} SYSTEM PRIVATE
            "${_ARMCLANG_NEWLIB_INC}"
        )
    endif()

    target_compile_definitions(${TGT} PRIVATE
        STM32H723xx USE_HAL_DRIVER USE_PWR_LDO_SUPPLY DATA_IN_D2_SRAM)

    target_compile_options(${TGT} PRIVATE
        -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16
        ${OPT_LIST} -g
        -ffunction-sections -fdata-sections ${_WARN_FLAGS}
    )

    if(STM32_STARM_CLANG)
        # starm-clang links with LLD against its own newlib sysroot. Give it the
        # Cortex-M7 (hard-float, fpv5-d16) multilib explicitly and group the
        # compiler builtins with libc/libm, mirroring the nano-f411 setup.
        # h7_tool_path keeps the path in the flavour the generated shell needs.
        h7_tool_path("${STARM_ROOT}/lib/clang-runtimes/newlib/arm-none-eabi/armv7m_hard_fpv5_d16_exn_rtti_unaligned_size/lib" _STARM_LIBDIR)
        if(NOT EXISTS "${_STARM_LIBDIR}/libc.a")
            message(FATAL_ERROR "starm-clang Cortex-M7 multilib not found at ${_STARM_LIBDIR} - check STARM_ROOT")
        endif()
        set(_LDFLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 ${OPT}")
        set(_LDFLAGS "${_LDFLAGS} -nostartfiles -Xlinker --gc-sections")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker -Map=${PROJECT_NAME}.map")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker -T -Xlinker ${H723_LINKER_SCRIPT}")
        set(_LDFLAGS "${_LDFLAGS} -L${_STARM_LIBDIR}")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker --start-group")
        set(_LDFLAGS "${_LDFLAGS} ${_STARM_LIBDIR}/libclang_rt.builtins.a -lc -lm")
        set(_LDFLAGS "${_LDFLAGS} -Xlinker --end-group")
    else()
        set(_LDFLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 ${OPT} -Wl,--gc-sections -nostartfiles -Wl,-Map=${PROJECT_NAME}.map -T ${H723_LINKER_SCRIPT} -lc -lm")
    endif()

    set_target_properties(${TGT} PROPERTIES
        LINK_FLAGS "${_LDFLAGS}"
    )

    # newlib/libgcc's thumb/v7e-m+fp multilib objects are built with
    # -fshort-enums and lack .note.GNU-stack, so a normal link spews ~60
    # benign warnings. Silence them (the sizes match the ARM EABI defaults
    # our objects use, so this is noise, not an ABI error). LLD (starm-clang)
    # does not know these GNU-ld-only switches, so skip them there.
    if(NOT STM32_STARM_CLANG)
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY
            LINK_FLAGS " -Wl,--no-enum-size-warning -Wl,--no-wchar-size-warning -Wl,--no-warn-execstack")
    endif()

    # Link-time optimization. armclang -flto emits LLVM bitcode (.llvm.lto) that
    # GNU ld cannot consume, so STM32_LTO is ignored there; starm-clang links
    # with LLD, which consumes bitcode natively (-flto=full).
    if(STM32_LTO AND STM32_STARM_CLANG)
        target_compile_options(${TGT} PRIVATE -flto=full -ffat-lto-objects)
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY LINK_FLAGS " -flto=full")
        # Keep the tiny newlib retarget layer out of LTO (same rationale as GCC).
        set_source_files_properties(${BOARD_DIR}/syscalls.c PROPERTIES
            COMPILE_OPTIONS "-fno-lto")
    elseif(STM32_LTO AND NOT STM32_ARMCLANG)
        target_compile_options(${TGT} PRIVATE -flto)
        set_property(TARGET ${TGT} APPEND_STRING PROPERTY LINK_FLAGS " -flto")
        # GCC LTO loses the newlib syscall-stub definitions (_fstat/_isatty/
        # _kill/_getpid) that live in syscalls.c: the plugin fails to resolve
        # the THM_CALL relocations from libc.a against the LTO IR, leaving
        # "undefined reference / Unknown destination type (ARM/Thumb)".
        # syscalls.c is a tiny retarget layer (not benchmark code), so compile
        # it without LTO.
        set_source_files_properties(${BOARD_DIR}/syscalls.c PROPERTIES
            COMPILE_OPTIONS "-fno-lto")
    endif()
endfunction()
