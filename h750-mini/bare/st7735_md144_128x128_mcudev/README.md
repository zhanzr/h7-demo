# st7735_md144_128x128_mcudev — ST7735S 1.44" 128x128 SPI LCD test (h750-mini)

Drives the **MD-144** 1.44-inch **ST7735S** 128x128 module in 4-line SPI mode
from the h750-mini @ 480 MHz. Ported from the hardware-proven nano-f411
project of the same name (f4-demo repo), rewired to the h750-mini **J1B**
expansion header. The demo loops forever through **two drive methods** — soft
bit-banged SPI and hardware **SPI4** @ 15 MHz — each running the full test
suite: banner, info pages (normal + inverted), the vendor TEST_STAND screens,
an HSV gradient sweep and the PA8 LED test, with a live FPS counter. The HW
pass also runs the backlight at 15 % (soft stays at 10 %) so the two are easy
to tell apart.

## This module is not the nano-f411's MD-144

The module (silk print **"mcudev"**, hence the project name) differs from the
nano-f411 reference panel in two measured ways:

| Difference | nano-f411 MD-144 | this mcudev module |
| ---------- | ---------------- | ------------------ |
| Visible window in GRAM | `ROW_Pre = 32` | **column 2, row 3** (`COL_Pre`/`ROW_Pre` in `lcd.h`) — with 0 the bottom band is visible, with 4 one uninitialised row shows at the top |
| HW SPI clocking | not re-checked | **SPI mode 0 only** (`CPOL=0`, `CPHA=1EDGE`). With `CPOL=1` the HW pass comes up **all-white** — see below |

### Why the HW path must use SPI mode 0

The panel latches SDA on the **rising** SCK edge, and modes 0 and 3 both
sample on that edge, so either *should* work. The catch is inside the HAL:
`HAL_SPI_Transmit()` ends by calling `SPI_CloseTransfer()`, which clears
`SPE`, i.e. **the SPI peripheral is disabled after every transfer.** With
`CPOL=1` the SCK pin snaps from its disabled level (low) up to its idle
level (high) as soon as `SPE` is set again — while `CS` is already asserted
— and the panel latches that extra edge as a 9th bit. Every byte then
arrives bit-shifted, so `0x11` (SLPOUT) and `0x29` (DISPON) never land and
the module stays uniformly white. `CPOL=0` keeps SCK low in both states, so
no spurious edge exists. The soft bit-bang was immune because it only ever
raises SCL with SDA already stable. Belt-and-braces: the SPI is also
configured with `MasterKeepIOState = ENABLE` (CFG2.AFCNTR), so the AF pins
stay driven at their idle level even while `SPE` is off (verified on
silicon: with the old config `CFG2`/`CR1` showed `AFCNTR=0, SPIEN=0` after
every transfer).

Speed: SPI4 runs from PCLK2 = 120 MHz. The default is **`/8` = 15 MHz**, the
ST7735S datasheet write-cycle limit (tSCYCW = 66 ns). Change `LCD_SPI4_PRESC`
in `interface.c`:

| Prescaler | SCK | Note |
| --------- | --- | ---- |
| `/2`  | 60 MHz | works for a while but 4x over the datasheet limit — not recommended |
| `/4`  | 30 MHz | 2x over the limit, testing only |
| `/8`  | **15 MHz** | current default, in spec |
| `/16` | 7.5 MHz | previous default, very conservative |
| `/32` | 3.75 MHz | only for long / loose wiring |

Measured full-screen gradient rate at `/2`: 148 fps HW vs 43 fps bit-bang; at
`/8` the bus is no longer the bottleneck on the 128x128 panel.

Everything else (init sequence `001_006_ST7735S_1.44_0xC8`, MADCTL 0xC8,
geometry, timing) carried over unchanged and works.

## Connection plan (all signals on J1B)

| LCD signal | MCU pin | J1B pin | Function |
| ---------- | ------- | ------- | -------- |
| LCD_CS     | PE11    | 36      | chip select (GPIO) |
| LCD_SCLK   | PE12    | 35      | SPI4_SCK (AF5), 15 MHz in HW mode |
| LCD_RST    | PE13    | 34      | reset (GPIO) |
| LCD_MOSI   | PE14    | 33      | SPI4_MOSI (AF5) |
| LCD_RS     | PE15    | 32      | data/command select (GPIO, 4-line SPI) |
| LCD_BLK    | PD15    | 18      | backlight, **TIM4_CH4 PWM @ 1 kHz, 10 % soft / 15 % HW** |

The five SPI/control signals sit on five consecutive J1B pins (32–36), and
the backlight PWM is PD15 (J1B 18) — the same TIM4_CH4 backlight pin the
`st7789_md200_240x320` demo uses. Module power: 3.3 V / GND from J1B.

Notes:

- The **soft bit-bang path is the fallback/reference**: the same wiring is
  re-muxed to plain GPIO and clocked by the 480 MHz core, so a module that
  fails on HW SPI can still be exercised.
- After every bus switch the panel is reset and re-initialized
  (`LCD_Reinit()`), so both passes start from a clean controller state.

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
| `src/main.c` | test-pattern suite (ported from the nano-f411 demo) |
| `src/lcd.c/h` | ST7735S driver: init sequence (vendor `001_006_ST7735S_1.44_0xC8`), drawing API, vendor screens |
| `src/interface.c/h` | 4-line SPI primitives: soft bit-bang / HW SPI4 runtime switching |
| `src/backlight.c/h` | LCD_BLK PWM (TIM4_CH4 @ PD15, 1 kHz) |
| `src/lcd/lcd_fonts*` | 6x12 + 8x16 ASCII fonts |
