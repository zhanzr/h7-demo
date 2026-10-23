/*
  st7789v_sx012_240x204 main for the h750-mini (STM32H750VBT6 @ 480 MHz).
  Same J1B wiring and driver structure as the st7735s_md100_128x96 project,
  for the "sx012" nearly-circle ST7789V module with a 240x204 panel
  (the ST7735S md100 firmware largely drives it, but the resolution /
  mirror / flips / init sequence must be the ST7789V panel's own - see
  lcd.c, vendor init 002_002_ST7789V_IPS.h: MADCTL 0x00, inversion ON).

  Vendor connections (13-pin, on an INVERTED convertor - module pin 1 is
  convertor pin 13, 2 -> 12, ..., 7 -> 7):
    1 LEDK1, 2 LEDK2 (cathodes -> GND), 3 LEDA (anode -> BLK PWM),
    4 GND, 5 FMARK (unused), 6 SDA, 7 RS, 8 SCL, 9 CS, 10 RESET,
    11 IOVCC (3.3 V), 12 VCC28 (3.3 V), 13 GND.

  MCU-side (same J1B pins as the st7735s_md100 project):
    CS=PE11(36) SCLK=PE12(35) RST=PE13(34) MOSI=PE14(33) RS/DC=PE15(32)
    BLK=PD15(18, TIM4_CH4 PWM, 10% soft / 15% HW)

  Clock: the shared board layer's 480 MHz tree is used as-is; SPI4 runs
  from PCLK2 = 120 MHz with the /8 prescaler = 15 MHz SCK (mode 0 - the
  same HAL/SPE spurious-edge constraint as the ST7735S projects; the
  ST7789V itself is happy at much higher rates - see st7789_md200).
*/

#include <stdio.h>
#include <string.h>
#include "board.h"
#include "lcd.h"
#include "interface.h"
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
#define INFO_DY    18             /* info-page line spacing (8x16 font)    */

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
/* Info page (ported from the md130 demo): compiler, build date, current
   frequency, driving method, the J1B wiring/backlight and the MCU unique
   device ID. `invert` swaps fg/bg (white background page).               */

/* Draw one info line horizontally centered (the "circle like" panel clips
 * the corners, so left-aligned text would be cut). The caller picks the
 * font; pass its width here (8 px for the 8x16 info font). */
static void info_line(uint16_t y, uint16_t glyph_w, const char *p)
{
    uint16_t x = (SCREEN_W - (uint16_t)strlen(p) * glyph_w) / 2;
    LCD_DisplayString(x, y, (char *)p);
}

static void info_demo(uint32_t ms, uint8_t invert)
{
    char buf[24];
    char comp[24];
    unsigned long mhz = (unsigned long)(SystemCoreClock / 1000000UL);
    uint16_t duty = Backlight_GetDuty();
    uint32_t fg = invert ? LCD_BLACK : LCD_WHITE;
    uint32_t bg = invert ? LCD_WHITE : LCD_BLACK;

    /* Bigger font for the round panel: the 8x16 glyphs are much easier to
     * read across the circle than the 6x12 ones. */
    pFONT *info_font = &ASCII_Font16;
    uint16_t glyph_w = info_font->Width;   /* 8 */

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

    /* The panel is "circle like": the bezel clips the corners, so every
     * info line is horizontally centered and the block sits in the
     * vertical middle of the content area instead of hugging the edge.
     * Lines are spaced INFO_DY apart with the 8x16 font (span
     * 5*INFO_DY + 16). */
    LCD_SetAsciiFont(info_font);
    int y = 2 + (ANIM_H - (5 * INFO_DY + 16)) / 2;
    info_line(y, glyph_w, comp);                              y += INFO_DY;

    snprintf(buf, sizeof buf, "Freq %lu MHz", mhz);
    info_line(y, glyph_w, buf);                               y += INFO_DY;

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
    info_line(y, glyph_w, buf);                               y += INFO_DY;

    snprintf(buf, sizeof buf, "J1B PE11-15 PD15");
    info_line(y, glyph_w, buf);                               y += INFO_DY;

    snprintf(buf, sizeof buf, "Build %s", __DATE__);
    info_line(y, glyph_w, buf);                               y += INFO_DY;

    snprintf(buf, sizeof buf, "BL=%u%%", (unsigned)duty);
    info_line(y, glyph_w, buf);

    LCD_SetAsciiFont(&ASCII_Font12);      /* back to the FPS-band font     */
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

    /* Big-font banner page: ASCII_Font32 (16x32 px glyphs), each line
     * centered on the round panel. Span 88 px (32 + 24 + 32), so the
     * block starts at y = 2 + (188 - 88) / 2 = 52. */
    LCD_SetAsciiFont(&ASCII_Font32);
    LCD_SetColor(fg);
    LCD_SetBackColor(bg);
    LCD_Clear();
    LCD_SetColor(bg);
    LCD_SetBackColor(bg);
    LCD_FillRect(0, ANIM_H, SCREEN_W, FPS_BAND);
    LCD_SetColor(fg);
    LCD_SetBackColor(bg);

    LCD_DisplayString((uint16_t)((SCREEN_W - strlen(l1) * 16) / 2), 52,
                      (char *)l1);
    LCD_DisplayString((uint16_t)((SCREEN_W - strlen(l2) * 16) / 2), 104,
                      (char *)l2);

    HAL_Delay(ms);

    LCD_SetAsciiFont(&ASCII_Font12);      /* back to the FPS-band font     */
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

    printf("\r\n==== h750-mini (STM32H750VBT6) st7789v_sx012_240x204 @ %lu MHz ====\r\n",
           (unsigned long)(SystemCoreClock / 1000000UL));
    printf("ST7789V nearly-circle 240x204 (sx012), 4-wire SPI, same J1B "
           "pins as st7735s_md100:\r\n");
    printf("CS=PE11(36) SCLK=PE12(35) RST=PE13(34) MOSI=PE14(33) "
           "RS/DC=PE15(32) BLK=PD15(18)\r\n");
    printf("Module signals: SDA(6) RS(7) SCL(8) CS(9) RESET(10) on the "
           "inverted convertor; MADCTL=0x00, IPS inversion on\r\n");

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
