#include "lcd_display.h"

#include <rtthread.h>
#include "hal_data.h"
#include <lcd_port.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifndef BSP_PLACE_IN_SECTION
#define BSP_PLACE_IN_SECTION(x)
#endif

#ifndef BSP_ALIGN_VARIABLE
#define BSP_ALIGN_VARIABLE(x)
#endif

#define LCD_COLOR_WHITE  ((uint16_t)0xFFFF)
#define LCD_COLOR_BLACK  ((uint16_t)0x0000)
#define DIGIT_WIDTH      8
#define DIGIT_HEIGHT     16
#define DIGIT_SCALE      4
#define DIGIT_SPACING    6
#define LCD_STRIDE_PIXELS DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0

static const uint8_t g_digit_font[11][DIGIT_HEIGHT] =
{
    {0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C, 0x00, 0x00}, /* 0 */
    {0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x7E, 0x00, 0x00}, /* 1 */
    {0x3C, 0x7E, 0xC3, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0xC3, 0xFF, 0xFF, 0x00, 0x00}, /* 2 */
    {0x3C, 0x7E, 0xC3, 0x03, 0x03, 0x1E, 0x1E, 0x03, 0x03, 0x03, 0x03, 0xC3, 0x7E, 0x3C, 0x00, 0x00}, /* 3 */
    {0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0x86, 0xFF, 0xFF, 0x06, 0x06, 0x06, 0x1F, 0x1F, 0x00, 0x00}, /* 4 */
    {0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xFC, 0xFE, 0x03, 0x03, 0x03, 0x03, 0xC3, 0x7E, 0x3C, 0x00, 0x00}, /* 5 */
    {0x1C, 0x38, 0x70, 0x60, 0xC0, 0xFC, 0xFE, 0xE3, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C, 0x00, 0x00}, /* 6 */
    {0xFF, 0xFF, 0x03, 0x06, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0x60, 0x00, 0x00}, /* 7 */
    {0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0x7E, 0x3C, 0x00, 0x00}, /* 8 */
    {0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xC3, 0xE7, 0x7F, 0x3F, 0x03, 0x06, 0x0E, 0x1C, 0x38, 0x00, 0x00}, /* 9 */
    {0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00}, /* - */
};

static uint16_t *lcd_display_fb(void)
{
    return (uint16_t *)&fb_background[0][0];
}

static void lcd_display_draw_pixel(int32_t x, int32_t y, uint16_t color)
{
    uint16_t *fb = lcd_display_fb();

    if ((x < 0) || (x >= LCD_WIDTH) || (y < 0) || (y >= LCD_HEIGHT))
    {
        return;
    }

    fb[(y * LCD_STRIDE_PIXELS) + x] = color;
}

static void lcd_display_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t color)
{
    for (int32_t py = 0; py < h; py++)
    {
        for (int32_t px = 0; px < w; px++)
        {
            lcd_display_draw_pixel(x + px, y + py, color);
        }
    }
}

static const uint8_t *lcd_display_get_glyph(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
    {
        return g_digit_font[ch - '0'];
    }

    if (ch == '-')
    {
        return g_digit_font[10];
    }

    return RT_NULL;
}

static void lcd_display_draw_char_scaled(int32_t x, int32_t y, char ch, uint16_t color, int32_t scale)
{
    const uint8_t *glyph = lcd_display_get_glyph(ch);

    if ((glyph == RT_NULL) || (scale < 1))
    {
        return;
    }

    for (int32_t row = 0; row < DIGIT_HEIGHT; row++)
    {
        uint8_t bits = glyph[row];

        for (int32_t col = 0; col < DIGIT_WIDTH; col++)
        {
            if (bits & (uint8_t)(0x80 >> col))
            {
                lcd_display_fill_rect(x + (col * scale), y + (row * scale), scale, scale, color);
            }
        }
    }
}

uint16_t *lcd_display_buffer_get(void)
{
    return lcd_display_fb();
}

void lcd_display_clear_white(void)
{
    uint16_t *fb = lcd_display_fb();

    for (int32_t i = 0; i < (LCD_WIDTH * LCD_HEIGHT); i++)
    {
        fb[i] = LCD_COLOR_WHITE;
    }
}

void lcd_display_present(void)
{
    /* Digits are drawn directly into the active display framebuffer. */
}

void lcd_display_show_number_text(const char *text)
{
    int32_t len;
    int32_t total_width;
    int32_t start_x;
    int32_t start_y;

    if (text == RT_NULL)
    {
        return;
    }

    len = (int32_t)strlen(text);
    if (len <= 0)
    {
        return;
    }

    lcd_display_clear_white();

    total_width = (len * DIGIT_WIDTH * DIGIT_SCALE) + ((len - 1) * DIGIT_SPACING);
    start_x = (LCD_WIDTH - total_width) / 2;
    start_y = (LCD_HEIGHT - (DIGIT_HEIGHT * DIGIT_SCALE)) / 2;

    for (int32_t i = 0; i < len; i++)
    {
        lcd_display_draw_char_scaled(start_x + i * ((DIGIT_WIDTH * DIGIT_SCALE) + DIGIT_SPACING),
                                     start_y,
                                     text[i],
                                     LCD_COLOR_BLACK,
                                     DIGIT_SCALE);
    }

    lcd_display_present();
}

void lcd_display_show_number(int32_t number)
{
    char text[16];

    rt_snprintf(text, sizeof(text), "%ld", (long)number);
    lcd_display_show_number_text(text);
}
