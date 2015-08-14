#include "piglcd.h"
#include <stdlib.h>
#include <string.h>


void write_pattern(struct PG_lcd_t *lcd, int cs)
{
    for(int column = 0 ; column < 64 ; ++column) {
        PG_lcd_pin_on(lcd, lcd->pin_rs);
        PG_lcd_select_chip(lcd, cs);

        if(column % 2 == 0) {
            char data = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6);
            PG_lcd_write_data_bit(lcd, data);
        } else {
            char data = (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7);
            PG_lcd_write_data_bit(lcd, data);
        }
        PG_lcd_pulse(lcd);

        PG_lcd_unselect_chip(lcd);
        PG_lcd_pin_off(lcd, lcd->pin_rs);
    }
}


int main()
{
    struct PG_lcd_t lcd;
    PG_lcd_initialize(&lcd);
    lcd.pin_rs = 24;
    lcd.pin_e = 26;
    lcd.pin_d0 = 3;
    lcd.pin_d1 = 5;
    lcd.pin_d2 = 7;
    lcd.pin_d3 = 11;
    lcd.pin_d4 = 13;
    lcd.pin_d5 = 15;
    lcd.pin_d6 = 19;
    lcd.pin_d7 = 21;
    lcd.pin_cs1 = 16;
    lcd.pin_cs2 = 18;
    lcd.pin_rst = 8;
    lcd.pin_led = 12;

    PG_lcd_setup(&lcd, PG_PINMAP_PHYS);
    PG_lcd_set_display_enable(&lcd, 1);
    PG_lcd_set_start_line(&lcd, 0);

    for(int i = 0 ; i < 50 ; ++i) {
        for(int page = 0 ; page < lcd.pages ; ++page) {
            PG_lcd_set_page(&lcd, 0, page);
            PG_lcd_set_column(&lcd, 0, 0);
            write_pattern(&lcd, 0);

            PG_lcd_set_page(&lcd, 1, page);
            PG_lcd_set_column(&lcd, 1, 0);
            write_pattern(&lcd, 1);
        }

        PG_lcd_clear_screen(&lcd);
    }
    
    PG_lcd_destroy(&lcd);
    
    return 0;
}
