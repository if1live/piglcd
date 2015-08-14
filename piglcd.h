#ifndef __PG_lcd_H__
#define __PG_lcd_H__

#include <stdint.h>

#define RPI 0
//#define RPI 1

typedef enum {
    PG_PINMAP_NORMAL,
    PG_PINMAP_GPIO,
    PG_PINMAP_PHYS,
    PG_PINMAP_SYS,
    PG_PINMAP_MAX_COUNT
} PG_pinmap_t;

struct PG_lcd_t {
    uint8_t rows;
    uint8_t columns;
    uint8_t pages;
    
    // pin number
    uint8_t pin_rs;
    uint8_t pin_e;
    uint8_t pin_d0;
    uint8_t pin_d1;
    uint8_t pin_d2;
    uint8_t pin_d3;
    uint8_t pin_d4;
    uint8_t pin_d5;
    uint8_t pin_d6;
    uint8_t pin_d7;
    uint8_t pin_cs1;
    uint8_t pin_cs2;
    uint8_t pin_rst;
    uint8_t pin_led;
    
    // data
    uint8_t *buffer;
};

void PG_lcd_initialize(struct PG_lcd_t *lcd);
void PG_lcd_destroy(struct PG_lcd_t *lcd);

void PG_lcd_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val);
void PG_lcd_pin_on(struct PG_lcd_t *lcd, uint8_t pin);
void PG_lcd_pin_off(struct PG_lcd_t *lcd, uint8_t pin);
void PG_lcd_pulse(struct PG_lcd_t *lcd);
void PG_lcd_reset(struct PG_lcd_t *lcd);

int PG_lcd_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type);
void PG_lcd_pin_all_low(struct PG_lcd_t *lcd);

void PG_lcd_set_page(struct PG_lcd_t *lcd, int chip, int page);
void PG_lcd_set_column(struct PG_lcd_t *lcd, int chip, int column);
void PG_lcd_set_display_enable(struct PG_lcd_t *lcd, int val);
void PG_lcd_set_start_line(struct PG_lcd_t *lcd, int idx);

void PG_lcd_select_chip(struct PG_lcd_t *lcd, int chip);
void PG_lcd_unselect_chip(struct PG_lcd_t *lcd);
void PG_lcd_write_data_bit(struct PG_lcd_t *lcd, uint8_t data);

void PG_lcd_clear_screen(struct PG_lcd_t *lcd);


// helper
#define UNUSED(x) (void)(x)

#endif  // __PG_lcd_H__
