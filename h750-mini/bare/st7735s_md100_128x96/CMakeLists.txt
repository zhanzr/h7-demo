# st7735s_md100_128x96: ST7735S 1.0" 128x96 SPI LCD test for the
# STM32H750VBTx (h750-mini) @ 480 MHz. Same J1B wiring and driver family as
# the st7735_md144_128x128_mcudev project (the bus is identical), for the
# smaller "md100" module: 128x96, two dead GRAM rows at the top skipped via
# ROW_Pre = 2, demo pages trimmed for the short panel.
#   CS=PE11(36) SCLK=PE12(35) RST=PE13(34) MOSI=PE14(33) RS/DC=PE15(32)
#   BLK=PD15(18, TIM4_CH4 PWM, 10% soft / 15% HW)
# Dual drive methods loop forever: soft bit-bang and hardware SPI4
# (15 MHz SCK, mode 0), each with the test suite + FPS counter.
#
# Pico-style workflow:
#   bash build.sh                           # default: GNU arm-none-eabi-gcc
#   mkdir -p build-ac6 && cd build-ac6      # optional: Keil AC6 (armclang)
#   cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..
#   ninja           # builds .elf + .hex (hex is what `ninja flash` uses)
#   ninja flash     # programs the board (probe-rs, ST-Link V2 / SWD)
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

project(st7735s_md100 C ASM)

if(STM32_TOOLCHAIN STREQUAL "armclang")
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/armclang-postproject.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/interface.c
    src/lcd.c
    src/backlight.c
    src/lcd/lcd_fonts.c
    src/lcd/lcd_font_1608.c
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/blockwrite
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/stm32h750_board.cmake)
stm32h750_apply_board(${PROJECT_NAME}.elf "-O2")

# The shared board layer only pulls in the HAL modules it needs; this project
# also uses SPI4 (ST7735S) and TIM4 (backlight PWM), so add their HAL sources.
target_sources(${PROJECT_NAME}.elf PRIVATE
    ${H750_HAL}/Src/stm32h7xx_hal_spi.c
    ${H750_HAL}/Src/stm32h7xx_hal_tim.c
    ${H750_HAL}/Src/stm32h7xx_hal_tim_ex.c
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
