/*
  st7735s_md100_128x96 main for the h750-mini (STM32H750VBT6 @ 480 MHz).
  Same J1B wiring and driver family as the st7735_md144_128x128_mcudev
  project, for the "md100" 1.0" ST7735S module with a 128x96 panel
  (measured: the mcudev 128x128 firmware largely works on it directly).

  96-row adaptations vs the md144 demo:
    - FPS band trimmed to 16 rows, info page trimmed to 5 lines
      (compiler / clock / bus / wiring+backlight / build date);
    - banner lines moved up for the shorter screen;
    - the top two GRAM rows of this module are dead (random noise) and
      are skipped via ROW_Pre = 2 in lcd.h.

    - big-font banner page (8x16 font), then the full pattern set:
      info page (normal + inverted), vendor TEST_STAND screens,
      HSV gradient sweep, LED test (PA8) - with a live FPS counter.
    - the same pattern set runs on BOTH drive methods, soft bit-bang and
      HW SPI4 (md130-style dual-bus loop).

  Wiring (identical to the md144 project, all on J1B):
    CS=PE11(36) SCLK=PE12(35) RST=PE13(34) MOSI=PE14(33) RS/DC=PE15(32)
    BLK=PD15(18, TIM4_CH4 PWM, 10% default)

  Clock: the shared board layer's 480 MHz tree is used as-is; SPI4 runs
  from PCLK2 = 120 MHz with the /8 prescaler = 15 MHz SCK, the ST7735S
  datasheet write-cycle limit (see LCD_SPI4_PRESC in interface.c).
  The HW path must use SPI
  **mode 0**: HAL_SPI_Transmit() disables SPE when it returns, so with
  CPOL=1 every re-enable puts an extra rising edge on SCK while CS is
  low, bit-shifting every byte - the panel then never receives 0x11 and
  stays all-white.
*/

#include <stdio.h>
#include "board.h"
#include "lcd.h"
#include "interface.h"
#include "lcd/lcd_font_1608.h"
#include "backlight.h"

/* ---- on-board LED: LD3 on PA8 (see blink_hello) ---- */
#define LED_PORT GPIOA
#define LED_PIN  GPIO_PIN_8
#define LED_ON()  HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET)
#define LED_OFF() HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET)

static void LED_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Pin   = LED_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &gpio);
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
}

#define SCREEN_W   LCD_Width     /* 128 */
#define SCREEN_H   LCD_Height    /* 96  */
#define FPS_BAND   16            /* bottom rows reserved for the FPS text   */
#define ANIM_H     (SCREEN_H - FPS_BAND)
#define BACK_COLOR LCD_BLACK
#define LED_HALF   1000          /* LED test dwell (ms)                    */

/* Backlight duty per drive method: HW runs slightly brighter so the two
 * passes are visually distinct. */
#define SOFT_BACKLIGHT_DUTY  10U  /* % */
#define HW_BACKLIGHT_DUTY    15U  /* % */

/* 96 px of content: with the 12 px font, 6 info lines (y = 2 + 5*12 = 62,
 * glyphs end at 74) fit above the 16-row FPS band. The md144 page's 8
 * lines (full wiring map, UID) do not fit on the short panel and are
 * reduced to the most important information. */
#define INFO_DY    12

/* --------------------------------------------------------------------- */
/* FPS counter.                                                          */
static volatile uint32_t g_frames;
static uint32_t         g_last_frames;
static uint32_t         g_fps_last_tick;
static uint32_t         g_fps_color = LCD_WHITE;   /* FPS glyph color     */

static void fps_frame(void)
{
    g_frames++;
}

static void fps_update(void)
{
    uint32_t now = HAL_GetTick();
    if (now - g_fps_last_tick >= 1000)
    {
        uint32_t fps = g_frames - g_last_frames;
        g_last_frames = g_frames;
        g_fps_last_tick = now;

        char buf[8];
        buf[0] = 'F'; buf[1] = 'P'; buf[2] = 'S'; buf[3] = ':';
        buf[4] = (char)('0' + (fps / 100) % 10);
        buf[5] = (char)('0' + (fps / 10) % 10);
        buf[6] = (char)('0' + fps % 10);
        buf[7] = '\0';
        LCD_SetColor(g_fps_color);
        LCD_ShowTransparent(1);              /* no opaque box */
        LCD_DisplayString(1, ANIM_H + 4, buf);
        LCD_ShowTransparent(0);

        /* Same figure on the console so the soft/HW throughput difference
         * is measurable without reading the panel (only while animating -
         * the static test screens report 0 every second otherwise). */
        if (fps != 0U)
        {
            printf("[LCD] fps=%lu bus=%s\r\n", (unsigned long)fps,
                   LCD_BusIsHw() ? "HW SPI4" : "soft");
        }
    }
}

static void paint_fps_band(void)
{
    LCD_SetColor(BACK_COLOR);
    LCD_SetBackColor(BACK_COLOR);
    LCD_FillRect(0, ANIM_H, SCREEN_W, FPS_BAND);
}

static void delay_with_fps(uint32_t ms)
{
    uint32_t start = HAL_GetTick();
    do
    {
        fps_update();
        HAL_Delay(50);
    } while (HAL_GetTick() - start < ms);
}

/* --------------------------------------------------------------------- */
/* Animated gradient: hue sweeps the full color wheel over `ms`.         */
static uint32_t hsv_to_rgb(int h, int s, int v)
{
    int region = (h / 600) % 6;
    int fpart  = h % 600;
    int p = v * (255 - s) / 255;
    int q = v * (255 - (s * fpart) / 600) / 255;
    int t = v * (255 - (s * (600 - fpart)) / 600) / 255;
    int r, g, b;
    switch (region)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default:r = v; g = p; b = q; break;
    }
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void draw_gradient(int hue_a, int hue_b, uint16_t *row)
{
    for (int y = 0; y < ANIM_H; y++)
    {
        int frac = y * 1000 / ANIM_H;
        int hue  = hue_a + (hue_b - hue_a) * frac / 1000;
        uint32_t c = hsv_to_rgb(hue, 255, 255);
        uint16_t rgb565 = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) |
                                     ((c >> 3) & 0x001F));
        for (int x = 0; x < SCREEN_W; x++)
        {
            row[x] = rgb565;
        }
        LCD_CopyBuffer(0, (uint16_t)y, SCREEN_W, 1, row);
    }
}

static void gradient_demo(uint32_t ms)
{
    static uint16_t row[SCREEN_W];
    LCD_SetBackColor(BACK_COLOR);
    paint_fps_band();

    uint32_t start = HAL_GetTick();
    uint32_t t = 0;
    do
    {
        int hue_a = (int)(t * 3600 / ms);
        int hue_b = hue_a + 1800;
        if (hue_b >= 3600) { hue_b -= 3600; }
        draw_gradient(hue_a, hue_b, row);
        fps_frame();
        fps_update();
        t = HAL_GetTick() - start;
    } while (t < ms);
}

/* --------------------------------------------------------------------- */
/* LED test (PA8, LD3).                                                  */
static void led_test(void)
{
    printf("[LCD] LED ON\r\n");
    LED_ON();
    delay_with_fps(LED_HALF);
    printf("[LCD] LED OFF\r\n");
    LED_OFF();
    delay_with_fps(LED_HALF);
}

/* --------------------------------------------------------------------- */
/* Vendor TEST_STAND screens (128x128).                                  */
static void TEST_STAND(void)
{
    DispFrame();
    StopDelay(Delay_Time);

    DispGrayHor16();
    StopDelay(Delay_Time);

    DispBand();
    StopDelay(Delay_Time);

    DispColor(RED);   StopDelay(Delay_Time);
    DispColor(GREEN); StopDelay(Delay_Time);
    DispColor(BLUE);  StopDelay(Delay_Time);
    DispColor(WHITE); StopDelay(Delay_Time);
    DispColor(BLACK); StopDelay(Delay_Time);
}

/* --------------------------------------------------------------------- */
/* Info page (ported from the md130 demo, trimmed to 5 lines for the
   96-row panel): compiler, build date, clock, driving method + SPI rate,
   and the J1B wiring/backlight. `invert` swaps fg/bg (white background). */
static void info_demo(uint32_t ms, uint8_t invert)
{
    char buf[24];
    char comp[24];
    unsigned long mhz = (unsigned long)(SystemCoreClock / 1000000UL);
    uint16_t duty = Backlight_GetDuty();
    uint32_t fg = invert ? LCD_BLACK : LCD_WHITE;
    uint32_t bg = invert ? LCD_WHITE : LCD_BLACK;

#if defined(__ARMCC_VERSION)
    snprintf(comp, sizeof comp, "AC6 %lu", (unsigned long)__ARMCC_VERSION);
#elif defined(__clang__)
    snprintf(comp, sizeof comp, "Clang %d.%d", __clang_major__, __clang_minor__);
#elif defined(__GNUC__)
    snprintf(comp, sizeof comp, "GCC %d.%d.%d",
             __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    snprintf(comp, sizeof comp, "unknown");
#endif

    printf("[LCD] info%s: compiler=%s build=%s %s\r\n",
           invert ? " (inverted)" : "", comp, __DATE__, __TIME__);
    printf("[LCD] info: freq=%lu MHz drive=%s BL=%u%%\r\n",
           mhz, LCD_BusIsHw() ? "HW SPI4" : "soft bit-bang", (unsigned)duty);

    LCD_SetColor(fg);
    LCD_SetBackColor(bg);
    LCD_Clear();

    /* FPS band in the page background color, then restore fg/bg. */
    LCD_SetColor(bg);
    LCD_SetBackColor(bg);
    LCD_FillRect(0, ANIM_H, SCREEN_W, FPS_BAND);
    LCD_SetColor(fg);
    LCD_SetBackColor(bg);
    g_fps_color = fg;                     /* FPS glyph matches the page */

    const int ix = 2;
    int y = 2;

    snprintf(buf, sizeof buf, "%s", comp);
    LCD_DisplayString(ix, (uint16_t)y, buf);  y += INFO_DY;

    snprintf(buf, sizeof buf, "Freq %lu MHz", mhz);
    LCD_DisplayString(ix, (uint16_t)y, buf);  y += INFO_DY;

    if (LCD_BusIsHw())
    {
        unsigned long khz = LCD_HwSpiKHz();
        snprintf(buf, sizeof buf, "SPI4 %lu.%lu MHz (HW)",
                 khz / 1000UL, (khz % 1000UL) / 100UL);
    }
    else
    {
        snprintf(buf, sizeof buf, "Bus soft (bit-bang)");
    }
    LCD_DisplayString(ix, (uint16_t)y, buf);  y += INFO_DY;

    snprintf(buf, sizeof buf, "J1B PE11-15 PD15");
    LCD_DisplayString(ix, (uint16_t)y, buf);  y += INFO_DY;

    snprintf(buf, sizeof buf, "Build %s", __DATE__);
    LCD_DisplayString(ix, (uint16_t)y, buf);  y += INFO_DY;

    snprintf(buf, sizeof buf, "BL=%u%%", (unsigned)duty);
    LCD_DisplayString(ix, (uint16_t)y, buf);

    g_fps_color = LCD_WHITE;              /* restore default FPS glyph color */

    uint32_t start = HAL_GetTick();
    do
    {
        fps_update();
        HAL_Delay(50);
    } while (HAL_GetTick() - start < ms);
}

/* --------------------------------------------------------------------- */
/* Big-font banner page (8x16 font).                                     */
static void banner_page(const char *l1, const char *l2,
                        uint32_t fg, uint32_t bg, uint32_t ms)
{
    g_fps_color = fg;

    LCD_SetAsciiFont(&ASCII_Font16);      /* bigger than the normal 6x12  */
    LCD_SetColor(fg);
    LCD_SetBackColor(bg);
    LCD_Clear();
    LCD_SetColor(bg);
    LCD_SetBackColor(bg);
    LCD_FillRect(0, ANIM_H, SCREEN_W, FPS_BAND);
    LCD_SetColor(fg);
    LCD_SetBackColor(bg);

    /* 8 px/glyph: center 13-char lines -> x = (128 - 13*8) / 2 = 12.
     * On the 96-row panel the two 16-px lines sit at y = 22/46, inside
     * the 80-row content area above the FPS band. */
    LCD_DisplayString(12, 22, (char *)l1);
    LCD_DisplayString(12, 46, (char *)l2);

    HAL_Delay(ms);

    LCD_SetAsciiFont(&ASCII_Font12);      /* back to the normal font      */
    g_fps_color = LCD_WHITE;
}

/* --------------------------------------------------------------------- */
/* Full test-pattern set (ported from the md130 demo).                   */
static void run_patterns(void)
{
    printf("[LCD] phase: info\r\n");
    info_demo(5000, 0);

    printf("[LCD] phase: info (inverted colors)\r\n");
    info_demo(5000, 1);

    printf("[LCD] phase: vendor TEST_STAND\r\n");
    TEST_STAND();

    printf("[LCD] phase: gradient\r\n");
    gradient_demo(1000);

    printf("[LCD] phase: LED test\r\n");
    led_test();
}

/* --------------------------------------------------------------------- */
int main(void)
{
    HAL_Init();
    Board_Init();
    LED_Init();

    printf("\r\n==== h750-mini (STM32H750VBT6) st7735s_md100_128x96 @ %lu MHz ====\r\n",
           (unsigned long)(SystemCoreClock / 1000000UL));
    printf("ST7735S 1.0\" 128x96 (md100), 4-wire SPI, same J1B wiring as "
           "st7735_md144_128x128_mcudev:\r\n");
    printf("CS=PE11(36) SCLK=PE12(35) RST=PE13(34) MOSI=PE14(33) "
           "RS/DC=PE15(32) BLK=PD15(18)\r\n");
    printf("Module-specific: 128x96; COL_Pre=%d ROW_Pre=%d (skip 2 dead "
           "rows at top); HW SPI4 @ 15 MHz, mode 0\r\n", COL_Pre, ROW_Pre);
    printf("Pages trimmed to 5 info lines / 16-row FPS band for the short "
           "panel\r\n");

    Backlight_Init();                     /* 10% default duty             */
    LCD_Init();
    LCD_SetAsciiFont(&ASCII_Font12);
    paint_fps_band();

    while (1)
    {
        /* ---- SOFT (bit-banged) bus ---- */
        printf("[LCD] phase: SOFT banner\r\n");
        LCD_UseSoftBus();
        Backlight_SetDuty(SOFT_BACKLIGHT_DUTY);
        LCD_Reinit();         /* re-frame the panel for the soft bus */
        banner_page("now will do", "soft SPI test",
                    LCD_YELLOW, LCD_BLUE, 3000);

        printf("[LCD] running patterns on SOFT SPI\r\n");
        run_patterns();

        /* ---- HARDWARE (SPI4) bus ---- */
        printf("[LCD] phase: HARDWARE banner\r\n");
        LCD_UseHwBus();
        /* Slightly brighter in HW mode so the two passes are easy to tell
         * apart at a glance (soft = 10%, HW = 15%). */
        Backlight_SetDuty(HW_BACKLIGHT_DUTY);
        LCD_Reinit();         /* re-frame the panel for the HW bus */
        banner_page("now will do", "HW SPI4 test",
                    LCD_BLACK, LCD_CYAN, 3000);

        printf("[LCD] running patterns on HARDWARE SPI4 @ %lu.%lu MHz\r\n",
               LCD_HwSpiKHz() / 1000UL, (LCD_HwSpiKHz() % 1000UL) / 100UL);
        run_patterns();
    }

    return 0;
}
