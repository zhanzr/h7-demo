# st7789_md200_240x320: ST7789 SPI LCD demo for the STM32H750VBTx (h750-mini) @ 480 MHz.
# Drives a 240x320 ST7789 panel over SPI4 (PE11 CS / PE12 SCK / PE14 MOSI,
# DC on PE15) with the TIM4_CH4 backlight on PD15. The demo loops through
# bouncing shapes, pure colors, an animated gradient, and an LED (PA8) test,
# while always showing an FPS counter at the bottom of the screen.
#
# Pico-style workflow:
#   bash build.sh                           # default: GNU arm-none-eabi-gcc
#   mkdir -p build-ac6 && cd build-ac6      # optional: Keil AC6 (armclang)
#   cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..
#   ninja           # builds .elf + .hex (hex is what `ninja flash` uses)
#   ninja flash     # programs the board (probe-rs, ULINK2 CMSIS-DAP / SWD)
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
    src/gpio.c
    src/spi.c
    src/tim.c
    src/lcd/lcd_spi_200.c
    src/lcd/lcd_fonts.c
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

stm32h750_apply_board(${PROJECT_NAME}.elf "-O2")

# QSPI app: make board.c's SystemClock_Config()/MPU a no-op so the identical
# main() can call Board_Init() without breaking the bootloader's clocks/MPU.
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE QSPI_APP=1)

# The shared board layer only pulls in the HAL modules it needs; this project
# also uses SPI4 (ST7789) and TIM4 (backlight PWM), so add their HAL sources.
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${H750_HAL}/Src/stm32h7xx_hal_spi.c
    ${H750_HAL}/Src/stm32h7xx_hal_tim.c
    ${H750_HAL}/Src/stm32h7xx_hal_tim_ex.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/qspi-flash-targets.cmake)
