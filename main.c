#include "piglcd.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main()
{
    struct PG_lcd_t lcd;
#ifdef __arm__
    PG_lcd_initialize(&lcd, PG_BACKEND_GPIO);
#else
    PG_lcd_initialize(&lcd, PG_BACKEND_GLFW);
#endif
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

    lcd.setup(&lcd, PG_PINMAP_PHYS);
    
    struct PG_framebuffer_t buffer;
    for(int i = 0 ; i < 50 ; ++i) {
        PG_framebuffer_write_sample_pattern(&buffer);
        PG_lcd_render_buffer(&lcd, &buffer);
        
        PG_framebuffer_clear(&buffer);
        PG_lcd_render_buffer(&lcd, &buffer);

        if(!lcd.is_alive(&lcd)) {
            break;
        }
    }

    PG_lcd_destroy(&lcd);

    return 0;
}
