#include "piglcd.h"
#include <stdlib.h>
#include <string.h>

#define RS 24
#define E 26
#define D0 3
#define D1 5
#define D2 7
#define D3 11
#define D4 13
#define D5 15
#define D6 19
#define D7 21
#define CS1 16
#define CS2 18
#define RST 8
#define LED 12

void write_pattern(struct glcd_t *glcd, int cs)
{
    for(int x = 0 ; x < 64 ; ++x) {
        glcd_pin_on(glcd, glcd->pin_rs);
        glcd_select_segment_unit(glcd, cs);

        if(x % 2 == 0) {
            char data = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6);
            glcd_write_data_bit(glcd, data);
        } else {
            char data = (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7);
            glcd_write_data_bit(glcd, data);
        }
        glcd_pulse(glcd);

        glcd_unselect_segment_unit(glcd);
        glcd_pin_off(glcd, glcd->pin_rs);
    }
}


int main()
{
    struct glcd_t glcd;
    memset(&glcd, 0, sizeof(glcd));
    glcd.pin_rs = RS;
    glcd.pin_e = E;
    glcd.pin_d0 = D0;
    glcd.pin_d1 = D1;
    glcd.pin_d2 = D2;
    glcd.pin_d3 = D3;
    glcd.pin_d4 = D4;
    glcd.pin_d5 = D5;
    glcd.pin_d6 = D6;
    glcd.pin_d7 = D7;
    glcd.pin_cs1 = CS1;
    glcd.pin_cs2 = CS2;
    glcd.pin_rst = RST;
    glcd.pin_led = LED;

    glcd_setup(&glcd);
    glcd_pin_all_low(&glcd);
    glcd_reset(&glcd);

    glcd_set_display_enable(&glcd, 1);
    glcd_set_start_line(&glcd, 0);

    int x = 0;
    for(int i = 0 ; i < 50 ; ++i) {
        for(x = 0 ; x < 8 ; ++x) {
            glcd_set_page(&glcd, 0, x);
            glcd_set_column(&glcd, 0, 0);
            write_pattern(&glcd, 0);

            glcd_set_page(&glcd, 1, x);
            glcd_set_column(&glcd, 1, 0);
            write_pattern(&glcd, 1);
        }

        glcd_clear_screen(&glcd);
    }
    return 0;
}
