# st7789_md200_240x320 — ST7789 SPI LCD demo for the STM32H750VBT6 (h750-mini) @ 480 MHz

Drives a **240×320 ST7789** panel over **SPI4** (CS PE11 / SCK PE12 / MOSI PE14,
DC PE15) with a **TIM4_CH4 PWM backlight** on PD15, on the 480 MHz h750-mini.
The LCD driver (`src/lcd/lcd_spi_200.c/.h` + `lcd_fonts.c/.h`) is the same one
used by the CubeMX/Keil project (`cubemx_file/`), just relocated; clock/MPU/caches/
UART/SysTick/startup all come from the shared board layer (`../board`, `../cmake`).

## Demo loop (repeats forever)

1. **Shapes (~5 s)** — squares, circles and triangles floating & bouncing,
   with the on-screen FPS counter at the bottom.
2. **Pure colors (5 s each)** — RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA,
   WHITE, BLACK.
3. **Gradient (~5 s)** — hue sweep through the full color wheel.
4. **LED test** — PA8 (LD3) LED: on, 5 s, off, 5 s.

An **FPS number is always shown at the bottom of the screen**, drawn
transparently (glyph pixels only). The bottom 24 rows are a reserved status
band so the counter survives the animation clears. Progress lines (phase
names, LED on/off) are also printed over USART1 (CH340 → `COM89` @ 115200).

On boot the **backlight brightness feature** is exercised (`lcd_bl_bright_set`):
full brightness, then stepped down — the PWM duty on PD15 is what changes.

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
bash tools/bench_capture.sh h750-mini/bare/st7789_md200_240x320/build/st7789.hex 70 st7789
```

Watch the demo phases on the panel and the phase lines on the serial console.
