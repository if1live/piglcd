#include "piglcd.h"
#include <stdio.h>

//#define RPI 0
#define RPI 1

#include <time.h>
#if RPI
#include <wiringPi.h>
#endif

#if RPI != 1
#define OUTPUT 1
void digitalWrite(int pin, int val) {}
void pinMode(int pin, int mode) {}
int wiringPiSetupPhys() { return 0; }
#endif

static void glcd_nanosleep(int nsec)
{
    struct timespec dt;
    struct timespec rmtp;
    dt.tv_sec = 0;
    dt.tv_nsec = nsec;
    nanosleep(&dt, &rmtp);
}

int glcd_setup(struct glcd_t *glcd)
{
    //WPI_MODE_glcd
    //if (wiringPiSetup() == -1) {
    //if (wiringPiSetupSys() == -1) {
    if (wiringPiSetupPhys() == -1) {
        //if (wiringPiSetupglcd() == -1) {
        return 1;
    }
    pinMode(glcd->pin_rs, OUTPUT);
    pinMode(glcd->pin_e, OUTPUT);
    pinMode(glcd->pin_d0, OUTPUT);
    pinMode(glcd->pin_d1, OUTPUT);
    pinMode(glcd->pin_d2, OUTPUT);
    pinMode(glcd->pin_d3, OUTPUT);
    pinMode(glcd->pin_d4, OUTPUT);
    pinMode(glcd->pin_d5, OUTPUT);
    pinMode(glcd->pin_d6, OUTPUT);
    pinMode(glcd->pin_d7, OUTPUT);
    pinMode(glcd->pin_cs1, OUTPUT);
    pinMode(glcd->pin_cs2, OUTPUT);
    pinMode(glcd->pin_rst, OUTPUT);
    pinMode(glcd->pin_led, OUTPUT);
    return 0;
}

void glcd_pin_all_low(struct glcd_t *glcd)
{
    int pin_array[] = {
        glcd->pin_rs,
        glcd->pin_e,
        glcd->pin_d0,
        glcd->pin_d1,
        glcd->pin_d2,
        glcd->pin_d3,
        glcd->pin_d4,
        glcd->pin_d5,
        glcd->pin_d6,
        glcd->pin_d7,
        glcd->pin_cs1,
        glcd->pin_cs2,
        glcd->pin_rst,
        glcd->pin_led,
    };
    for(int i = 0 ; i < sizeof(pin_array) / sizeof(pin_array[0]) ; ++i) {
        pin_pos_t pin = pin_array[i];
        glcd_pin_off(glcd, pin);
    }
}

void glcd_reset(struct glcd_t *glcd)
{
  glcd_pin_off(glcd, glcd->pin_rst);
  glcd_pin_on(glcd, glcd->pin_rst);
}

void glcd_pin_set_val(struct glcd_t *glcd, pin_pos_t pin, int val)
{
    digitalWrite(pin, val);
}

void glcd_pin_on(struct glcd_t *glcd, pin_pos_t pin)
{
    glcd_pin_set_val(glcd, pin, 1);
}

void glcd_pin_off(struct glcd_t *glcd, pin_pos_t pin)
{
    glcd_pin_set_val(glcd, pin, 0);
}

void glcd_pulse(struct glcd_t *glcd)
{
    glcd_pin_on(glcd, glcd->pin_e);

    // sleep short time
    glcd_nanosleep(1);

    glcd_pin_off(glcd, glcd->pin_e);
}

void glcd_set_page(struct glcd_t *glcd, int cs, int page)
{
    page = page & 0x07;
    glcd_select_segment_unit(glcd, cs);

    // 1 0 1 1 1 ? ? ?
    char data = page;
    data |= 1 << 7;
    data |= 1 << 5;
    data |= 1 << 4;
    data |= 1 << 3;
    glcd_write_data_bit(glcd, data);
    glcd_pulse(glcd);

    glcd_pin_off(glcd, glcd->pin_cs1);
    glcd_pin_off(glcd, glcd->pin_cs2);
}

void glcd_set_display_enable(struct glcd_t *glcd, int val)
{
    val = val & 0x01;

    glcd_pin_on(glcd, glcd->pin_cs1);
    glcd_pin_on(glcd, glcd->pin_cs2);

    // 0 0 1 1 1 1 1 ?
    char data = 0;
    data |= 1 << 5;
    data |= 1 << 4;
    data |= 1 << 3;
    data |= 1 << 2;
    data |= 1 << 1;
    if(val) {
        data |= 1;
    }

    glcd_write_data_bit(glcd, data);
    glcd_pulse(glcd);

    glcd_unselect_segment_unit(glcd);
}

void glcd_set_start_line(struct glcd_t *glcd, int idx)
{
    idx = idx & 0x3F;

    glcd_pin_on(glcd, glcd->pin_cs1);
    glcd_pin_on(glcd, glcd->pin_cs2);

    // 1 1 ? ?  ? ? ? ?
    char data = idx;
    data |= 1 << 7;
    data |= 1 << 6;
    glcd_write_data_bit(glcd, data);
    glcd_pulse(glcd);

    glcd_unselect_segment_unit(glcd);
}

void glcd_set_column(struct glcd_t *glcd, int cs, int column)
{
    column = column & 0x3F;

    glcd_select_segment_unit(glcd, cs);

    // 0 1 ? ? ? ? ? ?
    char data = column;
    data |= 1 << 6;
    glcd_write_data_bit(glcd, data);
    glcd_pulse(glcd);

    glcd_unselect_segment_unit(glcd);
}

void glcd_select_segment_unit(struct glcd_t *glcd, int cs)
{
    cs = cs & 0x01;

    if(cs == 0) {
        glcd_pin_on(glcd, glcd->pin_cs1);
    } else {
        glcd_pin_on(glcd, glcd->pin_cs2);
    }
}

void glcd_unselect_segment_unit(struct glcd_t *glcd)
{
    glcd_pin_off(glcd, glcd->pin_cs1);
    glcd_pin_off(glcd, glcd->pin_cs2);
}

void glcd_write_data_bit(struct glcd_t *glcd, char data)
{
    pin_pos_t data_pin_table[] = {
        glcd->pin_d0,
        glcd->pin_d1,
        glcd->pin_d2,
        glcd->pin_d3,
        glcd->pin_d4,
        glcd->pin_d5,
        glcd->pin_d6,
        glcd->pin_d7,
    };

    for(int i = 0 ; i < 8 ; ++i) {
        pin_pos_t pin = data_pin_table[i];
        char mask = 1 << i;
        if(data & mask) {
            glcd_pin_on(glcd, pin);
        } else {
            glcd_pin_off(glcd, pin);
        }
    }
}

void glcd_clear_screen(struct glcd_t *glcd)
{
    for(int x = 0 ; x < 8 ; ++x) {
        for(int cs = 0 ; cs < 2 ; ++cs) {
            glcd_set_page(glcd, cs, x);
            glcd_set_column(glcd, cs, 0);
            for(int y = 0 ; y < 64 ; ++y) {
                glcd_pin_on(glcd, glcd->pin_rs);
                glcd_select_segment_unit(glcd, cs);

                glcd_write_data_bit(glcd, 0);
                glcd_pulse(glcd);

                glcd_unselect_segment_unit(glcd);
                glcd_pin_off(glcd, glcd->pin_rs);
            }
        }
    }
}
