# ov5640_to_st7789 (h750-mini) - CMake build for the STM32H750VBTx.
#
# This is a GCC (arm-none-eabi) build that REUSES the CubeMX-generated sources
# from the sibling Keil project (../../cubemx_file). It does not need to
# be byte-identical to the Keil build; it follows the same memory map though
# (see stm32h750vbt.ld).
#
# Workflow (from this folder):
#   cmake -G Ninja -DCMAKE_MAKE_PROGRAM="D:/Program Files/Meson/ninja.exe" -B build
#   cmake --build build            # -> build/ov5640_to_st7789.elf/.hex/.bin
#   cmake --build build --target flash   # program via probe-rs + ULINK2 (SWD)

cmake_minimum_required(VERSION 3.13)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-toolchain.cmake")
endif()

project(ov5640_to_st7789 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- CubeMX-generated sources (shared with the Keil project) -----------------
set(H750_ROOT   ${CMAKE_CURRENT_SOURCE_DIR}/../../cubemx_file)
set(H750_CORE   ${H750_ROOT}/Core)
set(H750_DRV    ${H750_ROOT}/Drivers)
set(H750_HAL    ${H750_DRV}/STM32H7xx_HAL_Driver)
set(H750_CMSIS  ${H750_DRV}/CMSIS)
set(H750_CMSDEV ${H750_CMSIS}/Device/ST/STM32H7xx)

add_executable(${PROJECT_NAME}.elf

    # application (Core/Src)
    ${H750_CORE}/Src/main.c
    ${H750_CORE}/Src/adc.c
    ${H750_CORE}/Src/dcmi.c
    ${H750_CORE}/Src/dma.c
    ${H750_CORE}/Src/gpio.c
    ${H750_CORE}/Src/quadspi.c
    ${H750_CORE}/Src/spi.c
    ${H750_CORE}/Src/stm32h7xx_hal_msp.c
    ${H750_CORE}/Src/stm32h7xx_it.c
    ${H750_CORE}/Src/tim.c
    ${H750_CORE}/Src/usart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/system_app.c

    # application (Core/User, camera + LCD)
    ${H750_CORE}/User/dcmi_ov5640.c
    ${H750_CORE}/User/lcd_fonts.c
    ${H750_CORE}/User/lcd_spi_200.c
    ${H750_CORE}/User/sccb.c
    ${H750_CORE}/User/utils.c

    # STM32H7 HAL (same set the Keil project compiles)
    ${H750_HAL}/Src/stm32h7xx_hal.c
    ${H750_HAL}/Src/stm32h7xx_hal_adc.c
    ${H750_HAL}/Src/stm32h7xx_hal_adc_ex.c
    ${H750_HAL}/Src/stm32h7xx_hal_cortex.c
    ${H750_HAL}/Src/stm32h7xx_hal_dcmi.c
    ${H750_HAL}/Src/stm32h7xx_hal_dma.c
    ${H750_HAL}/Src/stm32h7xx_hal_dma_ex.c
    ${H750_HAL}/Src/stm32h7xx_hal_exti.c
    ${H750_HAL}/Src/stm32h7xx_hal_flash.c
    ${H750_HAL}/Src/stm32h7xx_hal_flash_ex.c
    ${H750_HAL}/Src/stm32h7xx_hal_gpio.c
    ${H750_HAL}/Src/stm32h7xx_hal_hsem.c
    ${H750_HAL}/Src/stm32h7xx_hal_i2c.c
    ${H750_HAL}/Src/stm32h7xx_hal_i2c_ex.c
    ${H750_HAL}/Src/stm32h7xx_hal_mdma.c
    ${H750_HAL}/Src/stm32h7xx_hal_pwr.c
    ${H750_HAL}/Src/stm32h7xx_hal_pwr_ex.c
    ${H750_HAL}/Src/stm32h7xx_hal_qspi.c
    ${H750_HAL}/Src/stm32h7xx_hal_rcc.c
    ${H750_HAL}/Src/stm32h7xx_hal_rcc_ex.c
    ${H750_HAL}/Src/stm32h7xx_hal_spi.c
    ${H750_HAL}/Src/stm32h7xx_hal_spi_ex.c
    ${H750_HAL}/Src/stm32h7xx_hal_tim.c
    ${H750_HAL}/Src/stm32h7xx_hal_tim_ex.c
    ${H750_HAL}/Src/stm32h7xx_hal_uart.c
    ${H750_HAL}/Src/stm32h7xx_hal_uart_ex.c
    ${H750_HAL}/Src/stm32h7xx_ll_delayblock.c

    # this project (build-specific: startup + newlib retarget)
    startup_stm32h750xx.s
    syscalls.c
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${H750_CORE}/Inc
    ${H750_CORE}/User
    ${H750_HAL}/Inc
    ${H750_HAL}/Inc/Legacy
    ${H750_CMSDEV}/Include
    ${H750_CMSIS}/Include
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    USE_HAL_DRIVER
    STM32H750xx
    USE_PWR_LDO_SUPPLY
    DATA_IN_D2_SRAM
    QSPI_APP
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16
    -O2 -g
    -ffunction-sections -fdata-sections
    -Wall
    -Wno-unused-but-set-variable -Wno-unused-function
    -Wno-unused-variable -Wno-unused-parameter -Wno-maybe-uninitialized
    # The shared CubeMX sources (camera/LCD) use %u/%X for uint32_t, which is
    # long on ARM; benign, and the code is shared with the Keil build - just
    # silence the format/strict-aliasing noise here.
    -Wno-format
    -Wno-strict-aliasing
)

set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    LINK_FLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 -O2 \
                -Wl,--gc-sections -nostartfiles -Wl,-Map=${PROJECT_NAME}.map \
                -T ${CMAKE_CURRENT_SOURCE_DIR}/app.ld -lc -lm"
)

# newlib/libgcc's thumb multilib objects are built with -fshort-enums and lack
# .note.GNU-stack, so a normal link spews benign warnings. Silence them.
set_property(TARGET ${PROJECT_NAME}.elf APPEND_STRING PROPERTY
    LINK_FLAGS " -Wl,--no-enum-size-warning -Wl,--no-wchar-size-warning -Wl,--no-warn-execstack")

# Images to the build dir + a `hex`/`bin` target (this is what `ninja flash`
# depends on), then the QUADSPI flash target.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)
