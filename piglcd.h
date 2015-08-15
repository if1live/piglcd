#ifndef __PG_lcd_H__
#define __PG_lcd_H__

#include <stdint.h>
#include <stdbool.h>
// for timespec
#include <time.h>

#define PG_ROWS 64
#define PG_COLUMNS 128
#define PG_PAGES (PG_ROWS / 8)
#define PG_CHIPS 2

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

struct PG_framebuffer_t {
    uint8_t data[PG_CHIPS][PG_PAGES][PG_COLUMNS/PG_CHIPS];
};
void PG_framebuffer_clear(struct PG_framebuffer_t *buffer);
void PG_framebuffer_write_sample_pattern(struct PG_framebuffer_t *buffer);


struct PG_lcd_t {
    PG_backend_t backend;
    
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
    struct PG_framebuffer_t buffer;
    
    // backend function
    void (*pin_set_val)(struct PG_lcd_t *lcd, uint8_t pin, int val);
    void (*pulse)(struct PG_lcd_t *lcd);
    int (*setup)(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type);
    int (*frame_end_callback)(struct PG_lcd_t *lcd);
    bool (*is_alive)(struct PG_lcd_t *lcd);
    
    // for glfw backend
    struct GLFWwindow *glfw_window;

    uint8_t glfw_val_rs;
    uint8_t glfw_val_e;
    uint8_t glfw_val_data_bits;
    uint8_t glfw_val_cs1;
    uint8_t glfw_val_cs2;
    uint8_t glfw_val_rst;
    uint8_t glfw_val_led;
    
    uint8_t glfw_state_display_enable;
    uint8_t glfw_state_page;
    uint8_t glfw_state_column;
    uint8_t glfw_state_chip;
    uint8_t glfw_state_start_line;
    
    struct PG_framebuffer_t glfw_framebuffer;
    
    // common
    struct timespec render_begin_tspec;
};

void PG_lcd_initialize(struct PG_lcd_t *lcd, PG_backend_t backend_type);
void PG_lcd_destroy(struct PG_lcd_t *lcd);

void PG_lcd_commit_buffer(struct PG_lcd_t *lcd);
void PG_lcd_render_buffer(struct PG_lcd_t *lcd, struct PG_framebuffer_t *buffer);

// helper
#define UNUSED(x) (void)(x)

#endif  // __PG_lcd_H__
