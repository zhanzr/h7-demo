# st7735s_md100_128x96 — ST7735S 1.0" 128x96 SPI LCD test (h750-mini)

Drives the **MD-100** 1.0-inch **ST7735S** **128x96** module in 4-line SPI
mode from the h750-mini @ 480 MHz. Same J1B wiring and driver family as
[`st7735_md144_128x128_mcudev`](../st7735_md144_128x128_mcudev/) — the mcudev
firmware largely works on this module directly. The demo loops forever
through **two drive methods** — soft bit-banged SPI and hardware **SPI4** @
15 MHz (mode 0) — each running the test suite: banner, info pages (normal +
inverted), the vendor TEST_STAND screens, an HSV gradient sweep and the PA8
LED test, with a live FPS counter.

## This module vs the mcudev MD-144

Measured on hardware, then confirmed against the vendor example
(`module_db/st7735s-md010-sqqvga3-st-01-v01`, `USER/main.h`):

| Difference | mcudev MD-144 (128x128) | this MD-100 (128x96) |
| ---------- | ----------------------- | -------------------- |
| Resolution | 128x128 | **128x96** |
| Visible GRAM window | column 2 / row 3 | **column 2 / row 1** — `COL_Pre = 2`, `ROW_Pre = 1` (vendor values) |

> ⚠ Raising `ROW_Pre` does **not** skip "dead rows": the top noise band is
> just the unwritten rows above the content, so it grows with the offset
> (observed 2 → 3 → 4 lines) while the picture bottom drops out of the
> window. The vendor's row-1 start leaves GRAM row 0 unwritten, which is
> the module's intended margin.

Everything else — init sequence `001_006_ST7735S_1.44_0xC8`, MADCTL 0xC8,
SPI **mode 0** requirement (see the mcudev README for the CPOL=1 / spurious
SPE-edge analysis), 15 MHz SCK — carried over unchanged and works.

## Pages trimmed for the 96-row panel

The md144 demo's 128-row layouts do not fit, so:

- FPS band trimmed 20 → **16 rows** (`ANIM_H` = 80 content rows);
- info page reduced from 8 lines to **5** — the most important information
  only: compiler, clock, driving method + SPI rate, J1B wiring + backlight
  duty, build date (the UID and the per-pin wiring rows were dropped; the
  wiring lives in this README and the console banner);
- the two banner lines moved to y = 22/46 so they stay inside the content
  area;
- vendor TEST_STAND screens and the gradient scale automatically with
  `COL`/`ROW`.

## Connection plan (identical to the md144 project, all on J1B)

| LCD signal | MCU pin | J1B pin | Function |
| ---------- | ------- | ------- | -------- |
| LCD_CS     | PE11    | 36      | chip select (GPIO) |
| LCD_SCLK   | PE12    | 35      | SPI4_SCK (AF5), 15 MHz in HW mode |
| LCD_RST    | PE13    | 34      | reset (GPIO) |
| LCD_MOSI   | PE14    | 33      | SPI4_MOSI (AF5) |
| LCD_RS     | PE15    | 32      | data/command select (GPIO, 4-line SPI) |
| LCD_BLK    | PD15    | 18      | backlight, **TIM4_CH4 PWM @ 1 kHz, 10 % soft / 15 % HW** |

Module power: 3.3 V / GND from J1B.

Notes:

- The **soft bit-bang path is the fallback/reference**; after every bus
  switch the panel is reset and re-initialized (`LCD_Reinit()`).
- HW SPI4 must stay in **mode 0** — same HAL/SPE spurious-edge issue as the
  md144 project (documented there and in `src/interface.c`).

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
| `src/main.c` | test-pattern suite (96-row layout) |
| `src/lcd.c/h` | ST7735S driver: init sequence (vendor `001_006_ST7735S_1.44_0xC8`), drawing API, vendor screens |
| `src/interface.c/h` | 4-line SPI primitives: soft bit-bang / HW SPI4 runtime switching |
| `src/backlight.c/h` | LCD_BLK PWM (TIM4_CH4 @ PD15, 1 kHz) |
| `src/lcd/lcd_fonts*` | 6x12 + 8x16 ASCII fonts |
