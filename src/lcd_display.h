#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <rtthread.h>
#include <stdint.h>

uint16_t *lcd_display_buffer_get(void);
void lcd_display_clear_white(void);
void lcd_display_present(void);
void lcd_display_show_number(int32_t number);
void lcd_display_show_number_text(const char *text);
void lcd_display_show_cn_text(const char *utf8_text);
void lcd_display_show_cn_text_at(int32_t x, int32_t y, const char *utf8_text);

#endif /* LCD_DISPLAY_H */
