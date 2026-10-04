# st7789_md154_240x240 — 1.54" 240x240 ST7789 SPI LCD demo for the STM32H723ZGT6 (h723-mini) @ 550 MHz

Drives the on-board **1.54" 240×240 ST7789** panel over **SPI6** (CS PG8 /
SCK PG13 / MOSI PG14, AF5, 68.75 MHz SCK) with the **backlight on PG12** and
**DC on PG15**. The LCD driver (`src/lcd/lcd_spi_154.c/.h` + `lcd_fonts.c/.h`)
is the vendor's `1.54寸240x240分辨率` example, relocated; clock/MPU/caches/UART/
SysTick/startup all come from the shared board layer (`../board`, `../cmake`).

## Demo loop (repeats forever)

1. **Shapes (~5 s)** — squares, circles and triangles floating & bouncing,
   with the on-screen FPS counter at the bottom.
2. **Pure colors (5 s each)** — RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA,
   WHITE, BLACK.
3. **Gradient (~5 s)** — hue sweep through the full color wheel.
4. **LED test** — PG7 (low-active) LED: on, 5 s, off, 5 s.

An **FPS number is always shown at the bottom of the screen**, drawn
transparently (glyph pixels only). The bottom 24 rows are a reserved status
band so the counter survives the animation clears. Progress lines (phase
names, LED on/off) are also printed over USART1 (`COMxx` @ 115200 via the
ST-Link V2's VCP).

The **backlight is a TIM23_CH1 PWM on PG12** (AF13), brightness set with
`lcd_bl_bright_set(duty)` (0..65535). The vendor example drove PG12 as a plain
GPIO, but the pin is wired to TIM23_CH1, so PWM dimming works. On boot it goes
full brightness for 200 ms, then settles at duty 11000 (~17 %).

## Build

```bash
bash build.sh                      # default: GNU arm-none-eabi-gcc == cmake -G Ninja .. && ninja

# Keil AC6 (armclang) in a separate build dir (CMAKE_TOOLCHAIN_FILE is cached)
mkdir -p build-ac6 && cd build-ac6
cmake -G Ninja -DSTM32_TOOLCHAIN=armclang ..
ninja
```

`ninja` builds the `.elf` + `.hex` (the `.hex` is what `ninja flash` programs);
`ninja bin` additionally writes a raw `.bin` image "in case" you need it.

## Flash + observe

```bash
ninja flash        # probe-rs through the ST-Link V2 (SWD)
```

Watch the demo phases on the panel and the phase lines on the serial console
(`COMxx` @ 115200).
