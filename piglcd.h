#ifndef __GLCD_H__
#define __GLCD_H__

typedef int pin_pos_t;
struct glcd_t {
    pin_pos_t pin_rs;
    pin_pos_t pin_e;
    pin_pos_t pin_d0;
    pin_pos_t pin_d1;
    pin_pos_t pin_d2;
    pin_pos_t pin_d3;
    pin_pos_t pin_d4;
    pin_pos_t pin_d5;
    pin_pos_t pin_d6;
    pin_pos_t pin_d7;
    pin_pos_t pin_cs1;
    pin_pos_t pin_cs2;
    pin_pos_t pin_rst;
    pin_pos_t pin_led;
};

void glcd_pin_set_val(struct glcd_t *glcd, pin_pos_t pin, int val);
void glcd_pin_on(struct glcd_t *glcd, pin_pos_t pin);
void glcd_pin_off(struct glcd_t *glcd, pin_pos_t pin);
void glcd_pulse(struct glcd_t *glcd);
void glcd_reset(struct glcd_t *glcd);

int glcd_setup(struct glcd_t *glcd);
void glcd_pin_all_low(struct glcd_t *glcd);

void glcd_set_page(struct glcd_t *glcd, int cs, int page);
void glcd_set_column(struct glcd_t *glcd, int cs, int column);
void glcd_set_display_enable(struct glcd_t *glcd, int val);
void glcd_set_start_line(struct glcd_t *glcd, int idx);

void glcd_select_segment_unit(struct glcd_t *glcd, int cs);
void glcd_unselect_segment_unit(struct glcd_t *glcd);
void glcd_write_data_bit(struct glcd_t *glcd, char data);

void glcd_clear_screen(struct glcd_t *glcd);

#endif  // __GLCD_H__
