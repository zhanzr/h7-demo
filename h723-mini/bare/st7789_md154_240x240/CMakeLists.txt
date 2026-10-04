# st7789_md154_240x240: 1.54" 240x240 ST7789 SPI LCD demo for the STM32H723ZGT6 (h723-mini) @ 550 MHz.
# Drives the on-board 1.54" 240x240 ST7789 panel over SPI6 (PG8 NSS / PG13 SCK /
# PG14 MOSI, AF5) with the backlight on PG12 and DC on PG15 (vendor 1.54寸
# example pinout). The demo loops through bouncing shapes, pure colors, an
# animated gradient, and an LED (PG7) test, while always showing an FPS
# counter at the bottom of the screen.
#
# Pico-style workflow:
#   bash build.sh                           # default: GNU arm-none-eabi-gcc
#   mkdir -p build-ac6 && cd build-ac6      # optional: Keil AC6 (armclang)
#   cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..
#   ninja           # builds .elf + .hex (hex is what `ninja flash` uses)
#   ninja flash     # programs the board (probe-rs, ST-Link V2 SWD)
#   ninja bin       # optional raw .bin image, "in case" you need it

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

# Toolchain selection. gcc (default) = GNU arm-none-eabi end-to-end;
# armclang = Keil Arm Compiler 6 C + GNU as/ld. CMAKE_TOOLCHAIN_FILE is
# cached, so use a separate build dir per toolchain (build/ for gcc,
# build-ac6/ for armclang).
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

project(st7789 C ASM)

if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/gpio.c
    src/tim.c
    src/lcd/lcd_spi_154.c
    src/lcd/lcd_fonts.c
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h723_board.cmake)
stm32h723_apply_board(${PROJECT_NAME}.elf "-O2")

# The shared board layer only pulls in the HAL modules it needs; this project
# also uses SPI6 (ST7789) and TIM23 (backlight PWM), so add their HAL sources.
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${H723_HAL}/Src/stm32h7xx_hal_spi.c
    ${H723_HAL}/Src/stm32h7xx_hal_tim.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
