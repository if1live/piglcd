#include "piglcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

// for glfw backend
#include <GLFW/glfw3.h>

// for gpip backend
#ifdef __arm__
#include <wiringPi.h>
#else
const int OUTPUT = 0;
static int wiringPiSetupMock()
{
    fprintf(stderr, "WiringPi not exist, use Mock.\n");
    return 0;
}
static int wiringPiSetup() { return wiringPiSetupMock(); }
static int wiringPiSetupGpio() { return wiringPiSetupMock(); }
static int wiringPiSetupPhys() { return wiringPiSetupMock(); }
static int wiringPiSetupSys() { return wiringPiSetupMock(); }

static void digitalWrite(int pin, int val) { UNUSED(pin); UNUSED(val); }
static void pinMode(int pin, int mode) { UNUSED(pin); UNUSED(mode); }
#endif

#define PIN_COUNT 14

// 1 0 1 1 1 ? ? ?
#define MASK_SET_PAGE 0b10111000
#define DATA_BITS_SET_PAGE 3

// 0 0 1 1 1 1 1 ?
#define MASK_SET_DISPLAY_ENABLE 0b00111110
#define DATA_BITS_SET_DISPLAY_ENABLE 1

// 1 1 ? ?  ? ? ? ?
#define MASK_SET_START_LINE 0b11000000
#define DATA_BITS_SET_START_LINE 6

// 0 1 ? ?  ? ? ? ?
#define MASK_SET_COLUMN 0b01000000
#define DATA_BITS_SET_COLUMN 6


static void PG_lcd_nanosleep(int nsec);
static void PG_lcd_fill_all_pin(struct PG_lcd_t *lcd, uint8_t *pin_table);

// gpio backend
static void PG_lcd_gpio_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val);
static void PG_lcd_gpio_pulse(struct PG_lcd_t *lcd);
static int PG_lcd_gpio_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type);
static int PG_lcd_gpio_frame_end_callback(struct PG_lcd_t *lcd);

// dummy backend
static void PG_lcd_dummy_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val);
static void PG_lcd_dummy_pulse(struct PG_lcd_t *lcd);
static int PG_lcd_dummy_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type);
static int PG_lcd_dummy_frame_end_callback(struct PG_lcd_t *lcd);

// glfw backend
static void PG_lcd_glfw_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val);
static void PG_lcd_glfw_pulse(struct PG_lcd_t *lcd);
static int PG_lcd_glfw_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type);
static int PG_lcd_glfw_frame_end_callback(struct PG_lcd_t *lcd);

// common function
void PG_lcd_pin_on(struct PG_lcd_t *lcd, uint8_t pin);
void PG_lcd_pin_off(struct PG_lcd_t *lcd, uint8_t pin);
void PG_lcd_pin_all_low(struct PG_lcd_t *lcd);
void PG_lcd_reset(struct PG_lcd_t *lcd);

void PG_lcd_set_page(struct PG_lcd_t *lcd, int chip, int page);
void PG_lcd_set_column(struct PG_lcd_t *lcd, int chip, int column);
void PG_lcd_set_display_enable(struct PG_lcd_t *lcd, int val);
void PG_lcd_set_start_line(struct PG_lcd_t *lcd, int idx);

void PG_lcd_select_chip(struct PG_lcd_t *lcd, int chip);
void PG_lcd_unselect_chip(struct PG_lcd_t *lcd);
void PG_lcd_write_data_bit(struct PG_lcd_t *lcd, uint8_t data);

// gpio backend
void PG_lcd_gpio_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val)
{
    UNUSED(lcd);
    digitalWrite(pin, val);
}
void PG_lcd_gpio_pulse(struct PG_lcd_t *lcd)
{
    PG_lcd_pin_on(lcd, lcd->pin_e);
    // sleep short time
    PG_lcd_nanosleep(1);
    PG_lcd_pin_off(lcd, lcd->pin_e);
}
int PG_lcd_gpio_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type)
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

    // common setup
    PG_lcd_pin_all_low(lcd);
    PG_lcd_reset(lcd);

    PG_lcd_set_display_enable(lcd, 1);
    PG_lcd_set_start_line(lcd, 0);

    return 0;
}
int PG_lcd_gpio_frame_end_callback(struct PG_lcd_t *lcd)
{
    UNUSED(lcd);
    return 0;
}

// dummy backend
void PG_lcd_dummy_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val)
{
    UNUSED(lcd);
    UNUSED(pin);
    UNUSED(val);
}
void PG_lcd_dummy_pulse(struct PG_lcd_t *lcd)
{
    UNUSED(lcd);
}
int PG_lcd_dummy_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type)
{
    UNUSED(lcd);
    UNUSED(pinmap_type);
    return 0;
}
int PG_lcd_dummy_frame_end_callback(struct PG_lcd_t *lcd)
{
    UNUSED(lcd);
    return 0;
}


// glfw backend
static GLFWwindow *glfw_window = NULL;

// LCD size
static int glfw_lcd_pixel_size = 6;
static int glfw_lcd_base_x = 0;
static int glfw_lcd_base_y = 0;
static int glfw_lcd_padding = 2;

static int glfw_val_rs = 0;
static int glfw_val_e = 0;
static int glfw_val_d0 = 0;
static int glfw_val_d1 = 0;
static int glfw_val_d2 = 0;
static int glfw_val_d3 = 0;
static int glfw_val_d4 = 0;
static int glfw_val_d5 = 0;
static int glfw_val_d6 = 0;
static int glfw_val_d7 = 0;
static int glfw_val_cs1 = 0;
static int glfw_val_cs2 = 0;
static int glfw_val_rst = 0;
static int glfw_val_led = 0;

static int glfw_state_display_enable = 0;
static int glfw_state_page = 0;
static int glfw_state_column = 0;
static int glfw_state_chip = 0;
static int glfw_state_start_line = 0;

static int glfw_state_grid[PG_DEFAULT_CHIPS][PG_DEFAULT_PAGES][PG_DEFAULT_COLUMNS/2] = {0};

void PG_lcd_glfw_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val)
{
    struct pin_tuple_t {
        uint8_t pin;
        int* addr;
    };
    struct pin_tuple_t pin_tuple_list[] = {
        { lcd->pin_rs, &glfw_val_rs },
        { lcd->pin_e, &glfw_val_e },
        { lcd->pin_d0, &glfw_val_d0 },
        { lcd->pin_d1, &glfw_val_d1 },
        { lcd->pin_d2, &glfw_val_d2 },
        { lcd->pin_d3, &glfw_val_d3 },
        { lcd->pin_d4, &glfw_val_d4 },
        { lcd->pin_d5, &glfw_val_d5 },
        { lcd->pin_d6, &glfw_val_d6 },
        { lcd->pin_d7, &glfw_val_d7 },
        { lcd->pin_cs1, &glfw_val_cs1 },
        { lcd->pin_cs2, &glfw_val_cs2 },
        { lcd->pin_rst, &glfw_val_rst },
        { lcd->pin_led, &glfw_val_led },
    };
    int count = sizeof(pin_tuple_list) / sizeof(pin_tuple_list[0]);
    for(int i = 0 ; i < count ; ++i) {
        if(pin_tuple_list[i].pin == pin) {
            *pin_tuple_list[i].addr = val;
            break;
        }
    }
}

void PG_lcd_glfw_pulse(struct PG_lcd_t *lcd)
{
    UNUSED(lcd);
    
    /*
    printf("RS  = %d\n", glfw_val_rs);
    printf("E   = %d\n", glfw_val_e);
    printf("D0  = %d\n", glfw_val_d0);
    printf("D1  = %d\n", glfw_val_d1);
    printf("D2  = %d\n", glfw_val_d2);
    printf("D3  = %d\n", glfw_val_d3);
    printf("D4  = %d\n", glfw_val_d4);
    printf("D5  = %d\n", glfw_val_d5);
    printf("D6  = %d\n", glfw_val_d6);
    printf("D7  = %d\n", glfw_val_d7);
    printf("CS1 = %d\n", glfw_val_cs1);
    printf("CS2 = %d\n", glfw_val_cs2);
    printf("RST = %d\n", glfw_val_rst);
    printf("LED = %d\n", glfw_val_led);
    */
    
    uint8_t data_bits = 0;
    uint8_t data_bit_val_list[] = {
        glfw_val_d0,
        glfw_val_d1,
        glfw_val_d2,
        glfw_val_d3,
        glfw_val_d4,
        glfw_val_d5,
        glfw_val_d6,
        glfw_val_d7
    };
    int data_bit_count = sizeof(data_bit_val_list) / sizeof(data_bit_val_list[0]);
    for(int i = 0 ; i < data_bit_count ; ++i) {
        data_bits |= (data_bit_val_list[i] << i);
    }
    
    if(glfw_val_cs1 == 1) {
        glfw_state_chip = 0;
    }
    if(glfw_val_cs2 == 1) {
        glfw_state_chip = 1;
    }
    //printf("curr chip : %d\n", glfw_state_chip);
    
    if(glfw_val_rs == 0) {
        int shift = 0;
        // display on/off
        shift = DATA_BITS_SET_DISPLAY_ENABLE;
        if((data_bits >> shift) == (MASK_SET_DISPLAY_ENABLE >> shift)) {
            glfw_state_display_enable = ((1 << shift) - 1) & data_bits;
            //printf("display on/off : %d\n", glfw_state_display_enable);
        }
        
        // set column
        shift = DATA_BITS_SET_COLUMN;
        if((data_bits >> shift) == (MASK_SET_COLUMN >> shift)) {
            glfw_state_column = ((1 << shift) - 1) & data_bits;
            //printf("set column : %d\n", glfw_state_column);
        }
        
        // set page
        shift = DATA_BITS_SET_PAGE;
        if((data_bits >> shift) == (MASK_SET_PAGE >> shift)) {
            glfw_state_page = ((1 << shift) - 1) & data_bits;
            //printf("set page : %d\n", glfw_state_page);
        }
        
        // set display start line
        shift = DATA_BITS_SET_START_LINE;
        if((data_bits >> shift) == (MASK_SET_START_LINE >> shift)) {
            glfw_state_start_line = ((1 << shift) - 1) & data_bits;
            //printf("set start line : %d\n", glfw_state_start_line);
        }
        
    } else {
        // write display data
        //printf("write data\n");
        glfw_state_grid[glfw_state_chip][glfw_state_page][glfw_state_column] = data_bits;
        
        int chip_columns = (lcd->columns / lcd->chips);
        glfw_state_column = (glfw_state_column + 1) % chip_columns;
    }
}

int PG_lcd_glfw_window_width(struct PG_lcd_t *lcd)
{
    int width = 0;
    width += glfw_lcd_pixel_size * lcd->columns;
    width += glfw_lcd_base_x * 2;
    width += glfw_lcd_padding * (lcd->columns - 1);
    return width;
}

int PG_lcd_glfw_window_height(struct PG_lcd_t *lcd)
{
    int height = 0;
    height += glfw_lcd_pixel_size * lcd->rows;
    height += glfw_lcd_base_y * 2;
    height += glfw_lcd_padding * (lcd->rows - 1);
    return height;
}

int PG_lcd_glfw_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type)
{
    UNUSED(pinmap_type);
    
    PG_lcd_pin_all_low(lcd);
    PG_lcd_reset(lcd);

    PG_lcd_set_display_enable(lcd, 1);
    PG_lcd_set_start_line(lcd, 0);

    // create glfw window
    if(!glfwInit()) {
        fprintf(stderr, "Cannot use glfw backend\n");
        exit(EXIT_FAILURE);
    }

    int width = PG_lcd_glfw_window_width(lcd);
    int height = PG_lcd_glfw_window_height(lcd);
    
    glfw_window = glfwCreateWindow(width, height, "GLFW Backend", NULL, NULL);
    if(!glfw_window) {
        glfwTerminate();
    }

    glfwMakeContextCurrent(glfw_window);
    glfwSwapInterval(1);

    return 0;
}
int PG_lcd_glfw_frame_end_callback(struct PG_lcd_t *lcd)
{
    UNUSED(lcd);

    int width = PG_lcd_glfw_window_width(lcd);
    int height = PG_lcd_glfw_window_height(lcd);

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if(!glfw_state_display_enable) {
        return 0;
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // gl 좌표계 뒤집기. 격자 그릴때는 y가 반대인게 편하다
    glOrtho(0, width, height, 0, 1.f, -1.f);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor3f(1.f, 1.f, 1.f);
    
    glBegin(GL_TRIANGLES);
    
    int chip_columns = (lcd->columns / lcd->chips);
    for(int chip = 0 ; chip < lcd->chips ; ++chip) {
        for(int page = 0 ; page < lcd->pages ; ++page) {
            for(int column = 0 ; column < chip_columns ; ++column) {
                uint8_t data = glfw_state_grid[chip][page][column];
                for(int i = 0 ; i < 8 ; ++i) {
                    uint8_t mask = (1 << i);
                    int val = data & mask;
                    if(!val) {
                        continue;
                    }
                    int x = chip * chip_columns + column;
                    int y = page * 8 + i;
                    
                    int l = glfw_lcd_base_x + x * (glfw_lcd_pixel_size + glfw_lcd_padding);
                    int t = glfw_lcd_base_y + y * (glfw_lcd_pixel_size + glfw_lcd_padding);
                    int r = l + glfw_lcd_pixel_size;
                    int b = t + glfw_lcd_pixel_size;
                    
                    glVertex3f(l, b, 0);
                    glVertex3f(r, b, 0);
                    glVertex3f(r, t, 0);
                    
                    glVertex3f(l, b, 0);
                    glVertex3f(r, t, 0);
                    glVertex3f(l, t, 0);
                }
            }
        }
    }
    
    glEnd();

    glfwSwapBuffers(glfw_window);
    glfwPollEvents();
    usleep(10 * 1000);
    return 0;
}

static void PG_lcd_nanosleep(int nsec)
{
    struct timespec dt;
    struct timespec rmtp;
    dt.tv_sec = 0;
    dt.tv_nsec = nsec;
    nanosleep(&dt, &rmtp);
}

void PG_lcd_fill_all_pin(struct PG_lcd_t *lcd, uint8_t *pin_table)
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

void PG_lcd_reset(struct PG_lcd_t *lcd)
{
    PG_lcd_pin_off(lcd, lcd->pin_rst);
    PG_lcd_pin_on(lcd, lcd->pin_rst);
}

void PG_lcd_initialize(struct PG_lcd_t *lcd, PG_backend_t backend_type)
{
    memset(lcd, 0, sizeof(*lcd));
    lcd->backend = backend_type;
    // set default value
    lcd->rows = PG_DEFAULT_ROWS;
    lcd->columns = PG_DEFAULT_COLUMNS;
    lcd->pages = PG_DEFAULT_PAGES;
    lcd->chips = PG_DEFAULT_CHIPS;

    // for backend
    switch(backend_type) {
        case PG_BACKEND_GPIO:
            lcd->pin_set_val = PG_lcd_gpio_pin_set_val;
            lcd->pulse = PG_lcd_gpio_pulse;
            lcd->setup = PG_lcd_gpio_setup;
            lcd->frame_end_callback = PG_lcd_gpio_frame_end_callback;
            break;
        case PG_BACKEND_GLFW:
            lcd->pin_set_val = PG_lcd_glfw_pin_set_val;
            lcd->pulse = PG_lcd_glfw_pulse;
            lcd->setup = PG_lcd_glfw_setup;
            lcd->frame_end_callback = PG_lcd_glfw_frame_end_callback;
            break;
        case PG_BACKEND_DUMMY:
            lcd->pin_set_val = PG_lcd_dummy_pin_set_val;
            lcd->pulse = PG_lcd_dummy_pulse;
            lcd->setup = PG_lcd_dummy_setup;
            lcd->frame_end_callback = PG_lcd_dummy_frame_end_callback;
            break;
        default:
            assert(!"invalid backend type");
            break;
    }

    lcd->buffer = (uint8_t*)malloc(sizeof(uint8_t) * lcd->columns * lcd->pages);
}

void PG_lcd_destroy(struct PG_lcd_t *lcd)
{
    if(lcd->buffer) {
        free(lcd->buffer);
        lcd->buffer = NULL;
    }
    if(glfw_window != NULL) {
        glfwDestroyWindow(glfw_window);
        glfwTerminate();
    }
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

void PG_lcd_pin_on(struct PG_lcd_t *lcd, uint8_t pin)
{
    lcd->pin_set_val(lcd, pin, 1);
}

void PG_lcd_pin_off(struct PG_lcd_t *lcd, uint8_t pin)
{
    lcd->pin_set_val(lcd, pin, 0);
}

void PG_lcd_set_page(struct PG_lcd_t *lcd, int chip, int page)
{
    page = page & (lcd->pages - 1);
    PG_lcd_select_chip(lcd, chip);

    uint8_t data = MASK_SET_PAGE | page;
    PG_lcd_write_data_bit(lcd, data);
    lcd->pulse(lcd);

    PG_lcd_pin_off(lcd, lcd->pin_cs1);
    PG_lcd_pin_off(lcd, lcd->pin_cs2);
}

void PG_lcd_set_display_enable(struct PG_lcd_t *lcd, int val)
{
    val = val & 0b1;
    PG_lcd_pin_on(lcd, lcd->pin_cs1);
    PG_lcd_pin_on(lcd, lcd->pin_cs2);

    // 0 0 1 1 1 1 1 ?
    uint8_t data = MASK_SET_DISPLAY_ENABLE;
    if(val) {
        data |= 1;
    }

    PG_lcd_write_data_bit(lcd, data);
    lcd->pulse(lcd);

    PG_lcd_unselect_chip(lcd);
}

void PG_lcd_set_start_line(struct PG_lcd_t *lcd, int idx)
{
    idx = idx & 0b111111;

    PG_lcd_pin_on(lcd, lcd->pin_cs1);
    PG_lcd_pin_on(lcd, lcd->pin_cs2);

    // 1 1 ? ?  ? ? ? ?
    uint8_t data = MASK_SET_START_LINE | idx;
    PG_lcd_write_data_bit(lcd, data);
    lcd->pulse(lcd);

    PG_lcd_unselect_chip(lcd);
}

void PG_lcd_set_column(struct PG_lcd_t *lcd, int chip, int column)
{
    column = column & 0b111111;

    PG_lcd_select_chip(lcd, chip);

    // 0 1 ? ? ? ? ? ?
    uint8_t data = MASK_SET_COLUMN | column;
    PG_lcd_write_data_bit(lcd, data);
    lcd->pulse(lcd);

    PG_lcd_unselect_chip(lcd);
}

void PG_lcd_select_chip(struct PG_lcd_t *lcd, int chip)
{
    chip = chip & 0b1;

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
        for(int chip = 0 ; chip < lcd->chips ; ++chip) {
            PG_lcd_set_page(lcd, chip, page);
            PG_lcd_set_column(lcd, chip, 0);
            for(int column = 0 ; column < (lcd->columns / lcd->chips) ; ++column) {
                PG_lcd_pin_on(lcd, lcd->pin_rs);
                PG_lcd_select_chip(lcd, chip);

                PG_lcd_write_data_bit(lcd, 0);
                lcd->pulse(lcd);

                PG_lcd_unselect_chip(lcd);
                PG_lcd_pin_off(lcd, lcd->pin_rs);
            }
        }
    }
    lcd->frame_end_callback(lcd);
}

void PG_lcd_write_test_pattern(struct PG_lcd_t *lcd)
{
    for(int page = 0 ; page < lcd->pages ; ++page) {
        for(int chip = 0 ; chip < lcd->chips ; ++chip) {
            PG_lcd_set_page(lcd, chip, page);
            PG_lcd_set_column(lcd, chip, 0);
            for(int column = 0 ; column < (lcd->columns / lcd->chips) ; ++column) {
                PG_lcd_pin_on(lcd, lcd->pin_rs);
                PG_lcd_select_chip(lcd, chip);

                if(column % 2 == 0) {
                    char data = 0b01010101;
                    PG_lcd_write_data_bit(lcd, data);
                } else {
                    char data = 0b10101010;
                    PG_lcd_write_data_bit(lcd, data);
                }
                lcd->pulse(lcd);

                PG_lcd_unselect_chip(lcd);
                PG_lcd_pin_off(lcd, lcd->pin_rs);
            }
        }
    }
    lcd->frame_end_callback(lcd);
}
