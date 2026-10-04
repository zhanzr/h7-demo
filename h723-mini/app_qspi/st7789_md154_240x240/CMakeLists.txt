# st7789_md154_240x240 (app_qspi): ST7789 LCD demo (1.54" 240x240) from the W25Q64.
# Same sources as bare/st7789_md154_240x240 (SPI6, TIM23 PWM backlight), linked
# at 0x90000000 and booted by h723_boot.
#
# Pico-style workflow:
#   bash build.sh
#   ninja           # builds .elf + .hex
#   ninja flash     # writes the W25Q64 via the OCTOSPI flash algorithm

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

project(st7789 C ASM)

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
    src/gpio.c
    src/tim.c
    src/lcd/lcd_spi_154.c
    src/lcd/lcd_fonts.c
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

stm32h723_apply_board(${PROJECT_NAME}.elf "-O2")

target_sources(${PROJECT_NAME}.elf PRIVATE
    ${H723_HAL}/Src/stm32h7xx_hal_spi.c
    ${H723_HAL}/Src/stm32h7xx_hal_tim.c
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE QSPI_APP=1)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)
