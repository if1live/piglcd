#ifndef __PG_lcd_H__
#define __PG_lcd_H__

#include <stdint.h>


typedef enum {
    PG_PINMAP_NORMAL,
    PG_PINMAP_GPIO,
    PG_PINMAP_PHYS,
    PG_PINMAP_SYS,
    PG_PINMAP_MAX_COUNT
} PG_pinmap_t;

typedef enum {
    PG_BACKEND_GPIO,
    PG_BACKEND_GLFW,
    PG_BACKEND_DUMMY,
    PG_BACKEND_MAX_COUNT,
} PG_backend_t;

struct PG_lcd_t {
    uint8_t rows;
    uint8_t columns;
    uint8_t pages;
    uint8_t chips;
    
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
    
    // backend function
    void (*pin_set_val)(struct PG_lcd_t *lcd, uint8_t pin, int val);
    void (*pulse)(struct PG_lcd_t *lcd);
    int (*setup)(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type);
};

void PG_lcd_initialize(struct PG_lcd_t *lcd, PG_backend_t backend_type);
void PG_lcd_destroy(struct PG_lcd_t *lcd);

void PG_lcd_clear_screen(struct PG_lcd_t *lcd);
void PG_lcd_write_test_pattern(struct PG_lcd_t *lcd);

// helper
#define UNUSED(x) (void)(x)

#endif  // __PG_lcd_H__
