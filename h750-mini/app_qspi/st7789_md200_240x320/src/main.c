/**
  * @file    main.c
  * @brief   st7789 LCD test demo on the h750-mini (STM32H750VBTx @ 480 MHz).
  *
  * Main loop:
  *   1. floating & bouncing squares / circles / triangles          (~5 s)
  *   2. pure colors, each shown for 5 s                            (RED..BLACK)
  *   3. animated gradient sweeping through the color wheel         (~5 s)
  *   4. LED (PA8) test: toggle, 5 s, toggle, 5 s
  *   ... repeat forever.
  *
  * An FPS counter is always shown at the bottom of the screen,
  * drawn transparently (glyph pixels only, no opaque box).
  *
  * The bottom 24 rows are reserved as a status band so the FPS text is not
  * wiped by the animation clears; animations are clipped to that area.
  */

#include <stdio.h>

#include "board.h"
#include "uart_printf.h"
#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"
#include "lcd/lcd_spi_200.h"

#define SCREEN_W   LCD_Width   /* 240 */
#define SCREEN_H   LCD_Height  /* 320 */
#define FPS_BAND   24          /* bottom rows reserved for the FPS text  */
#define ANIM_H     (SCREEN_H - FPS_BAND)
#define BACK_COLOR LCD_BLACK

/* ------------------------------------------------------------------------ */
/* FPS: g_frames is incremented once per rendered animation frame;
 * fps_update() redraws the on-screen counter once per second.             */
static volatile uint32_t g_frames;
static uint32_t         g_last_frames;
static uint32_t         g_fps_last_tick;

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

        /* Manual %03d: armclang specializes snprintf() into the ARMCLIB ABI
         * (__2snprintf), which the newlib link cannot resolve. */
        char buf[8];
        buf[0] = 'F';
        buf[1] = 'P';
        buf[2] = 'S';
        buf[3] = ':';
        buf[4] = (char)('0' + (fps / 100) % 10);
        buf[5] = (char)('0' + (fps / 10) % 10);
        buf[6] = (char)('0' + fps % 10);
        buf[7] = '\0';
        LCD_SetColor(LCD_WHITE);
        LCD_ShowTransparent(1);                 /* no opaque box */
        LCD_DisplayString(0, ANIM_H, buf);
        LCD_ShowTransparent(0);
    }
}

/* Paint the reserved bottom band (solid, behind the FPS text). */
static void paint_fps_band(void)
{
    LCD_SetColor(BACK_COLOR);
    LCD_SetBackColor(BACK_COLOR);
    LCD_FillRect(0, ANIM_H, SCREEN_W, FPS_BAND);
}

/* Delay helper that keeps the on-screen FPS counter live. */
static void delay_with_fps(uint32_t ms)
{
    uint32_t start = HAL_GetTick();
    do
    {
        fps_update();
        HAL_Delay(50);
    } while (HAL_GetTick() - start < ms);
}

/* ------------------------------------------------------------------------ */
/* Floating & bouncing shapes.                                               */
typedef enum { SHAPE_SQUARE, SHAPE_CIRCLE, SHAPE_TRIANGLE } shape_kind_t;

typedef struct
{
    shape_kind_t kind;
    int          x, y;    /* center */
    int          vx, vy;
    int          size;    /* half size / radius */
    uint32_t     color;
} shape_t;

static const uint32_t shape_palette[] = {
    LCD_RED, LCD_GREEN, LCD_BLUE, LCD_YELLOW,
    LCD_CYAN, LCD_MAGENTA, LCD_WHITE,
};

static void init_shapes(shape_t *s, int n)
{
    static const shape_kind_t kinds[3] = { SHAPE_SQUARE, SHAPE_CIRCLE, SHAPE_TRIANGLE };
    for (int i = 0; i < n; i++)
    {
        s[i].kind  = kinds[i % 3];
        s[i].x     = 20 + (i * 53) % (SCREEN_W - 40);
        s[i].y     = 20 + (i * 97) % (ANIM_H - 60);
        s[i].vx    = (i % 2 ? 1 : -1) * (2 + (i % 4));
        s[i].vy    = (i % 3 ? 1 : -1) * (2 + (i % 5));
        s[i].size  = 14 + (i % 4) * 5;
        s[i].color = shape_palette[i % (sizeof(shape_palette) / sizeof(shape_palette[0]))];
    }
}

static void draw_shape(const shape_t *s)
{
    LCD_SetColor(s->color);
    switch (s->kind)
    {
    case SHAPE_SQUARE:
        LCD_FillRect(s->x - s->size, s->y - s->size, 2 * s->size, 2 * s->size);
        break;
    case SHAPE_CIRCLE:
        LCD_FillCircle(s->x, s->y, s->size);
        break;
    default:
    {
        int r = s->size;
        int x0 = s->x,      y0 = s->y - r;              /* top            */
        int x1 = s->x - r,  y1 = s->y + (r * 8) / 10;   /* bottom left    */
        int x2 = s->x + r,  y2 = s->y + (r * 8) / 10;   /* bottom right   */
        LCD_DrawLine(x0, y0, x1, y1);
        LCD_DrawLine(x1, y1, x2, y2);
        LCD_DrawLine(x2, y2, x0, y0);
        break;
    }
    }
}

static void step_shape(shape_t *s)
{
    s->x += s->vx;
    s->y += s->vy;
    int r = s->size;
    if (s->x - r < 0)             { s->x = r;                 s->vx = -s->vx; }
    if (s->x + r > SCREEN_W - 1)  { s->x = SCREEN_W - 1 - r;  s->vx = -s->vx; }
    if (s->y - r < 0)             { s->y = r;                 s->vy = -s->vy; }
    if (s->y + r > ANIM_H - 1)    { s->y = ANIM_H - 1 - r;    s->vy = -s->vy; }
}

static void shapes_demo(uint32_t ms)
{
    enum { N = 8 };
    shape_t shapes[N];
    init_shapes(shapes, N);

    LCD_SetBackColor(BACK_COLOR);
    paint_fps_band();

    uint32_t start = HAL_GetTick();
    do
    {
        LCD_ClearRect(0, 0, SCREEN_W, ANIM_H);
        for (int i = 0; i < N; i++)
        {
            step_shape(&shapes[i]);
            draw_shape(&shapes[i]);
        }
        fps_frame();
        fps_update();
    } while (HAL_GetTick() - start < ms);
}

/* ------------------------------------------------------------------------ */
/* Pure colors, one after the other.                                         */
static void colors_demo(uint32_t ms_per_color)
{
    static const uint32_t colors[] = {
        LCD_RED, LCD_GREEN, LCD_BLUE, LCD_YELLOW,
        LCD_CYAN, LCD_MAGENTA, LCD_WHITE, LCD_BLACK,
    };

    for (unsigned i = 0; i < sizeof(colors) / sizeof(colors[0]); i++)
    {
        LCD_SetColor(colors[i]);
        LCD_SetBackColor(colors[i]);
        LCD_Clear();
        paint_fps_band();
        printf("[LCD] pure color %u\r\n", (unsigned)i + 1);
        delay_with_fps(ms_per_color);
    }
}

/* ------------------------------------------------------------------------ */
/* Animated gradient: hue sweeps the full color wheel over `ms`.             */
static uint32_t hsv_to_rgb(int h, int s, int v)
{
    /* h: 0..3600 (0.1 deg), s/v: 0..255 */
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
        int frac = y * 1000 / ANIM_H;               /* 0..1000 across height */
        int hue  = hue_a + (hue_b - hue_a) * frac / 1000;
        uint32_t c = hsv_to_rgb(hue, 255, 255);
        uint16_t rgb565 = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
        for (int x = 0; x < SCREEN_W; x++)
        {
            row[x] = rgb565;
        }
        LCD_CopyBuffer(0, y, SCREEN_W, 1, row);
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
        int hue_a = (int)(t * 3600 / ms);           /* full sweep over ms */
        int hue_b = hue_a + 1800;                   /* complementary      */
        if (hue_b >= 3600) hue_b -= 3600;
        draw_gradient(hue_a, hue_b, row);
        fps_frame();
        fps_update();
        t = HAL_GetTick() - start;
    } while (t < ms);
}

/* ------------------------------------------------------------------------ */
static void led_test(void)
{
    printf("[LCD] LED ON\r\n");
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    delay_with_fps(5000);
    printf("[LCD] LED OFF\r\n");
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    delay_with_fps(5000);
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    HAL_Init();
    Board_Init();

    MX_GPIO_Init();
    MX_SPI4_Init();
    MX_TIM4_Init();
    SPI_LCD_Init();         /* also starts the backlight (lcd_bl_bright_set) */
    LCD_SetAsciiFont(&ASCII_Font24);

    /* Backlight "brightness feature" demo: full -> off -> breathe. */
    lcd_bl_bright_set(30000);
    HAL_Delay(200);
    lcd_bl_bright_set(11000);

    printf("\r\n=== st7789 LCD test on STM32H750VBTx @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);

    paint_fps_band();

    while (1)
    {
        printf("[LCD] phase: shapes\r\n");
        shapes_demo(5000);

        printf("[LCD] phase: pure colors\r\n");
        colors_demo(5000);

        printf("[LCD] phase: gradient\r\n");
        gradient_demo(5000);

        printf("[LCD] phase: LED test\r\n");
        led_test();
    }

    return 0;
}
