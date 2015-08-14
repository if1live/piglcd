#include "piglcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#if RPI
#include <wiringPi.h>
#endif

#define PIN_COUNT 14

#if RPI != 1
#define OUTPUT 1
void digitalWrite(int pin, int val) 
{
    UNUSED(pin);
    UNUSED(val);
}
void pinMode(int pin, int mode)
{
    UNUSED(pin);
    UNUSED(mode);
}

int wiringPiSetup() { return 0; }
int wiringPiSetupGpio() { return 0; }
int wiringPiSetupPhys() { return 0; }
int wiringPiSetupSys() { return 0; }
#endif

static void PG_lcd_nanosleep(int nsec)
{
    struct timespec dt;
    struct timespec rmtp;
    dt.tv_sec = 0;
    dt.tv_nsec = nsec;
    nanosleep(&dt, &rmtp);
}

static void PG_lcd_fill_all_pin(struct PG_lcd_t *lcd, uint8_t *pin_table) 
{
    int i = 0 ;
    *(pin_table + i++) = lcd->pin_rs;
    *(pin_table + i++) = lcd->pin_e;
    *(pin_table + i++) = lcd->pin_d0;
    *(pin_table + i++) = lcd->pin_d1;
    *(pin_table + i++) = lcd->pin_d2;
    *(pin_table + i++) = lcd->pin_d3;
    *(pin_table + i++) = lcd->pin_d4;
    *(pin_table + i++) = lcd->pin_d5;
    *(pin_table + i++) = lcd->pin_d6;
    *(pin_table + i++) = lcd->pin_d7;
    *(pin_table + i++) = lcd->pin_cs1;
    *(pin_table + i++) = lcd->pin_cs2;
    *(pin_table + i++) = lcd->pin_rst;
    *(pin_table + i++) = lcd->pin_led;
}

void PG_lcd_initialize(struct PG_lcd_t *lcd)
{
    memset(lcd, 0, sizeof(*lcd));
    // set default value
    lcd->rows = 64;
    lcd->columns = 128;
    lcd->pages = lcd->rows / 8;
    
    lcd->buffer = (uint8_t*)malloc(sizeof(uint8_t) * lcd->columns * lcd->pages);
}

void PG_lcd_destroy(struct PG_lcd_t *lcd)
{
    if(lcd->buffer) {
        free(lcd->buffer);
        lcd->buffer = 0;
    }
}

int PG_lcd_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type)
{
    int success = -1;
    switch(pinmap_type) {
        case PG_PINMAP_NORMAL:
            success = wiringPiSetup();
            break;
        case PG_PINMAP_GPIO:
            success = wiringPiSetupGpio();
            break;
        case PG_PINMAP_PHYS:
            success = wiringPiSetupPhys();
            break;
        case PG_PINMAP_SYS:
            success = wiringPiSetupSys();
            break;
        default:
            return -1;
    }
    
    if(success == -1) {
        return 1;
    }
    
    uint8_t pin_array[PIN_COUNT];
    PG_lcd_fill_all_pin(lcd, pin_array);
    for(int i = 0 ; i < PIN_COUNT ; ++i) {
        uint8_t pin = pin_array[i];
        pinMode(pin, OUTPUT);
    }

    PG_lcd_pin_all_low(lcd);
    PG_lcd_reset(lcd);
    
    return 0;
}

void PG_lcd_pin_all_low(struct PG_lcd_t *lcd)
{
    uint8_t pin_array[PIN_COUNT];
    PG_lcd_fill_all_pin(lcd, pin_array);
    for(int i = 0 ; i < PIN_COUNT ; ++i) {
        uint8_t pin = pin_array[i];
        PG_lcd_pin_off(lcd, pin);
    }
}

void PG_lcd_reset(struct PG_lcd_t *lcd)
{
  PG_lcd_pin_off(lcd, lcd->pin_rst);
  PG_lcd_pin_on(lcd, lcd->pin_rst);
}

void PG_lcd_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val)
{
    UNUSED(lcd);
    digitalWrite(pin, val);
}

void PG_lcd_pin_on(struct PG_lcd_t *lcd, uint8_t pin)
{
    PG_lcd_pin_set_val(lcd, pin, 1);
}

void PG_lcd_pin_off(struct PG_lcd_t *lcd, uint8_t pin)
{
    PG_lcd_pin_set_val(lcd, pin, 0);
}

void PG_lcd_pulse(struct PG_lcd_t *lcd)
{
    PG_lcd_pin_on(lcd, lcd->pin_e);

    // sleep short time
    PG_lcd_nanosleep(1);

    PG_lcd_pin_off(lcd, lcd->pin_e);
}

void PG_lcd_set_page(struct PG_lcd_t *lcd, int chip, int page)
{
    page = page & (lcd->pages - 1);
    PG_lcd_select_chip(lcd, chip);

    // 1 0 1 1 1 ? ? ?
    uint8_t data = (1 << 7) | (1 << 5) | (1 << 4) | (1 << 3) | page;
    PG_lcd_write_data_bit(lcd, data);
    PG_lcd_pulse(lcd);

    PG_lcd_pin_off(lcd, lcd->pin_cs1);
    PG_lcd_pin_off(lcd, lcd->pin_cs2);
}

void PG_lcd_set_display_enable(struct PG_lcd_t *lcd, int val)
{
    val = val & 0x01;

    PG_lcd_pin_on(lcd, lcd->pin_cs1);
    PG_lcd_pin_on(lcd, lcd->pin_cs2);

    // 0 0 1 1 1 1 1 ?
    uint8_t data = (1 << 5) | (1 << 4) | (1 << 3) | (1 << 2) | (1 << 1);
    if(val) {
        data |= 1;
    }

    PG_lcd_write_data_bit(lcd, data);
    PG_lcd_pulse(lcd);

    PG_lcd_unselect_chip(lcd);
}

void PG_lcd_set_start_line(struct PG_lcd_t *lcd, int idx)
{
    idx = idx & 0x3F;

    PG_lcd_pin_on(lcd, lcd->pin_cs1);
    PG_lcd_pin_on(lcd, lcd->pin_cs2);

    // 1 1 ? ?  ? ? ? ?
    uint8_t data = (1 << 7) | (1 << 6) | idx;
    PG_lcd_write_data_bit(lcd, data);
    PG_lcd_pulse(lcd);

    PG_lcd_unselect_chip(lcd);
}

void PG_lcd_set_column(struct PG_lcd_t *lcd, int chip, int column)
{
    column = column & 0x3F;

    PG_lcd_select_chip(lcd, chip);

    // 0 1 ? ? ? ? ? ?
    uint8_t data = (1 << 6) | column;
    PG_lcd_write_data_bit(lcd, data);
    PG_lcd_pulse(lcd);

    PG_lcd_unselect_chip(lcd);
}

void PG_lcd_select_chip(struct PG_lcd_t *lcd, int chip)
{
    chip = chip & 0x01;

    if(chip == 0) {
        PG_lcd_pin_on(lcd, lcd->pin_cs1);
    } else {
        PG_lcd_pin_on(lcd, lcd->pin_cs2);
    }
}

void PG_lcd_unselect_chip(struct PG_lcd_t *lcd)
{
    PG_lcd_pin_off(lcd, lcd->pin_cs1);
    PG_lcd_pin_off(lcd, lcd->pin_cs2);
}

void PG_lcd_write_data_bit(struct PG_lcd_t *lcd, uint8_t data)
{
    uint8_t data_pin_table[] = {
        lcd->pin_d0,
        lcd->pin_d1,
        lcd->pin_d2,
        lcd->pin_d3,
        lcd->pin_d4,
        lcd->pin_d5,
        lcd->pin_d6,
        lcd->pin_d7,
    };

    for(int i = 0 ; i < 8 ; ++i) {
        uint8_t pin = data_pin_table[i];
        uint8_t mask = 1 << i;
        if(data & mask) {
            PG_lcd_pin_on(lcd, pin);
        } else {
            PG_lcd_pin_off(lcd, pin);
        }
    }
}

void PG_lcd_clear_screen(struct PG_lcd_t *lcd)
{
    for(int page = 0 ; page < lcd->pages ; ++page) {
        for(int chip = 0 ; chip < 2 ; ++chip) {
            PG_lcd_set_page(lcd, chip, page);
            PG_lcd_set_column(lcd, chip, 0);
            for(int column = 0 ; column < 64 ; ++column) {
                PG_lcd_pin_on(lcd, lcd->pin_rs);
                PG_lcd_select_chip(lcd, chip);

                PG_lcd_write_data_bit(lcd, 0);
                PG_lcd_pulse(lcd);

                PG_lcd_unselect_chip(lcd);
                PG_lcd_pin_off(lcd, lcd->pin_rs);
            }
        }
    }
}
