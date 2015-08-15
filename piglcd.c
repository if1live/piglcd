#include "piglcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

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
static GLFWwindow *g_window = NULL;
void PG_lcd_glfw_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val)
{
    UNUSED(lcd);
    UNUSED(pin);
    UNUSED(val);
}
void PG_lcd_glfw_pulse(struct PG_lcd_t *lcd)
{
    UNUSED(lcd);
}
int PG_lcd_glfw_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type)
{
    UNUSED(lcd);
    UNUSED(pinmap_type);
    
    // create glfw window
    if(!glfwInit()) {
        exit(EXIT_FAILURE);
    }
    
    g_window = glfwCreateWindow(640, 480, "GLFW Backend", NULL, NULL);
    if(!g_window) {
        glfwTerminate();
    }
    
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);
    
    return 0;
}
int PG_lcd_glfw_frame_end_callback(struct PG_lcd_t *lcd)
{
    UNUSED(lcd);
    
    float ratio;
    int width, height;

    glfwGetFramebufferSize(g_window, &width, &height);
    ratio = width / (float) height;

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();
    glRotatef((float) glfwGetTime() * 50.f, 0.f, 0.f, 1.f);

    glBegin(GL_TRIANGLES);
    glColor3f(1.f, 0.f, 0.f);
    glVertex3f(-0.6f, -0.4f, 0.f);
    glColor3f(0.f, 1.f, 0.f);
    glVertex3f(0.6f, -0.4f, 0.f);
    glColor3f(0.f, 0.f, 1.f);
    glVertex3f(0.f, 0.6f, 0.f);
    glEnd();

    glfwSwapBuffers(g_window);
    glfwPollEvents();
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
    lcd->rows = 64;
    lcd->columns = 128;
    lcd->pages = lcd->rows / 8;
    lcd->chips = 2;
    
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
    if(g_window != NULL) {
        glfwDestroyWindow(g_window);
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

    // 1 0 1 1 1 ? ? ?
    uint8_t data = (1 << 7) | (1 << 5) | (1 << 4) | (1 << 3) | page;
    PG_lcd_write_data_bit(lcd, data);
    lcd->pulse(lcd);

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
    lcd->pulse(lcd);

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
    lcd->pulse(lcd);

    PG_lcd_unselect_chip(lcd);
}

void PG_lcd_set_column(struct PG_lcd_t *lcd, int chip, int column)
{
    column = column & 0x3F;

    PG_lcd_select_chip(lcd, chip);

    // 0 1 ? ? ? ? ? ?
    uint8_t data = (1 << 6) | column;
    PG_lcd_write_data_bit(lcd, data);
    lcd->pulse(lcd);

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
            for(int column = 0 ; column < 64 ; ++column) {
                PG_lcd_pin_on(lcd, lcd->pin_rs);
                PG_lcd_select_chip(lcd, chip);
        
                if(column % 2 == 0) {
                    char data = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6);
                    PG_lcd_write_data_bit(lcd, data);
                } else {
                    char data = (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7);
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