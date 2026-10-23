# st7789v_sx012_240x204 — ST7789V "nearly circle" 240x204 SPI LCD test (h750-mini)

Drives the **SX012** "nearly circle" **ST7789V** module (240x204 visible) in
4-line SPI mode from the h750-mini @ 480 MHz. Same J1B wiring and driver
structure as [`st7735s_md100_128x96`](../st7735s_md100_128x96/) — the
ST7735S md100 firmware largely drives the ST7789V, but the geometry /
mirror / flips and the init sequence must be this panel's own, taken from
the vendor example
(`module_db/st7789v-sx012-qvga4-13p-01-v01`, init `002_002_ST7789V_IPS.h`).

The demo loops forever through **two drive methods** — soft bit-banged SPI
and hardware **SPI4** @ 15 MHz (mode 0) — each running the test suite:
banner, info pages (normal + inverted), the vendor TEST_STAND screens, an
HSV gradient sweep and the PA8 LED test, with a live FPS counter.

## Module configuration (from the vendor example)

| Item | Value |
| ---- | ----- |
| Geometry | `COL = 240`, `ROW = 204`, `COL_Pre = 0`, `ROW_Pre = 0` |
| MADCTL (0x36) | **0x00** — no mirror / flip |
| Display inversion | **ON (`0x21`)** — IPS panel |
| Pixel format | RGB565 (`0x3A = 0x05`) |
| Init sequence | `002_002_ST7789V_IPS.h`: Sleep In (`0x10`) first, ST7789V porch / power / VCOM / gamma tables, Sleep Out (`0x11`) + Display On (`0x29`) last |

Delays (120 ms after `0x10` and `0x11`) were added around the vendor's
verbatim commands per the ST7789V datasheet; the vendor file relies on its
own caller-side timing.

## Connection plan

Vendor 13-pin list, mounted on an **inverted convertor** (module pin 1 is
convertor pin 13, 2 → 12, …, 7 → 7 — the convertor pin is `14 − module pin`):

| Module pin | Signal | Connects to | Convertor pin |
| ---------- | ------ | ----------- | ------------- |
| 1 / 2 | LEDK1 / LEDK2 (cathodes) | GND | 13 / 12 |
| 3 | LEDA (anode) | **LCD_BLK**: PD15 (J1B 18), TIM4_CH4 PWM @ 1 kHz, 10 % soft / 15 % HW | 11 |
| 4 / 13 | GND | GND | 10 / 1 |
| 5 | FMARK | unused (per the vendor note) | 9 |
| 6 | SDA | PE14 (J1B 33), SPI4_MOSI | 8 |
| 7 | RS | PE15 (J1B 32), data/command (GPIO) | 7 |
| 8 | SCL | PE12 (J1B 35), SPI4_SCK (AF5), 15 MHz | 6 |
| 9 | CS | PE11 (J1B 36), chip select (GPIO) | 5 |
| 10 | RESET | PE13 (J1B 34), reset (GPIO) | 4 |
| 11 / 12 | IOVCC / VCC28 | 3.3 V | 3 / 2 |

So the MCU-side wiring is identical to the st7735s_md100 project; only the
convertor's pin numbering is inverted.

## Bus / speed

HW mode is **SPI mode 0** (the same HAL/SPE spurious-edge constraint as the
ST7735S projects — see the mcudev README) at PCLK2 (120 MHz) / 8 =
**15 MHz**. The ST7789V is much faster than that in principle — the
`st7789_md200_240x320` demo runs SPI4 at 60 MHz mode 0 on the same pins —
so `LCD_SPI4_PRESC` can be raised once the convertor wiring proves it
(e.g. `/4` = 30 MHz, or `/2` = 60 MHz).

## Build

```bash
bash build.sh                       # default: GNU arm-none-eabi-gcc
ninja flash                         # probe-rs, ST-Link V2 (SWD)
```

Console: USART1 via the on-board CH340 @ 115200 (`COM3` on this machine) —
each demo phase prints a `[LCD] phase: ...` line.

## Files

| File | Role |
| ---- | ---- |
| `src/main.c` | test-pattern suite (240x204 layout) |
| `src/lcd.c/h` | ST7789V driver: vendor `002_002_ST7789V_IPS` init, drawing API, vendor screens |
| `src/interface.c/h` | 4-line SPI primitives: soft bit-bang / HW SPI4 runtime switching |
| `src/backlight.c/h` | LCD_BLK PWM (TIM4_CH4 @ PD15, 1 kHz) |
| `src/lcd/lcd_fonts*` | 6x12 + 8x16 ASCII fonts |
