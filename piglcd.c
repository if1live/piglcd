#include "piglcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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
#define DATA_PIN_COUNT 8

// LCD size
#define GLFW_LCD_BASE_X 0
#define GLFW_LCD_BASE_Y 0
#define GLFW_LCD_PADDING 2
#define GLFW_LCD_PIXEL_SIZE 6

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

// http://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x
#ifdef __MACH__
#include <sys/time.h>
#define CLOCK_MONOTONIC 0
//clock_gettime is not implemented on OSX
int clock_gettime(int clk_id, struct timespec* t) {
    UNUSED(clk_id);
    struct timeval now;
    int rv = gettimeofday(&now, NULL);
    if (rv) return rv;
    t->tv_sec  = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
}
#endif

// helper function
static struct timespec PG_timespec_subtract(struct timespec *a, struct timespec *b);
static void PG_nanosleep(int nsec);

static void PG_lcd_fill_all_pin(struct PG_lcd_t *lcd, uint8_t pin_table[PIN_COUNT]);
static void PG_lcd_fill_data_pin(struct PG_lcd_t *lcd, uint8_t pin_table[DATA_PIN_COUNT]);

// gpio backend
static void PG_lcd_gpio_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val);
static void PG_lcd_gpio_pulse(struct PG_lcd_t *lcd);
static int PG_lcd_gpio_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type);
static int PG_lcd_gpio_frame_end_callback(struct PG_lcd_t *lcd);
static bool PG_lcd_gpio_is_alive(struct PG_lcd_t *lcd);

// dummy backend
static void PG_lcd_dummy_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val);
static void PG_lcd_dummy_pulse(struct PG_lcd_t *lcd);
static int PG_lcd_dummy_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type);
static int PG_lcd_dummy_frame_end_callback(struct PG_lcd_t *lcd);
static bool PG_lcd_dummy_is_alive(struct PG_lcd_t *lcd);

// glfw backend
static void PG_lcd_glfw_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val);
static void PG_lcd_glfw_pulse(struct PG_lcd_t *lcd);
static int PG_lcd_glfw_setup(struct PG_lcd_t *lcd, PG_pinmap_t pinmap_type);
static int PG_lcd_glfw_frame_end_callback(struct PG_lcd_t *lcd);
static bool PG_lcd_glfw_is_alive(struct PG_lcd_t *lcd);

// common function
void PG_lcd_pin_on(struct PG_lcd_t *lcd, uint8_t pin);
void PG_lcd_pin_off(struct PG_lcd_t *lcd, uint8_t pin);
void PG_lcd_pin_all_low(struct PG_lcd_t *lcd);
void PG_lcd_reset(struct PG_lcd_t *lcd);

void PG_lcd_set_page(struct PG_lcd_t *lcd, int page);
void PG_lcd_set_column(struct PG_lcd_t *lcd, int column);
void PG_lcd_set_display_enable(struct PG_lcd_t *lcd, int val);
void PG_lcd_set_start_line(struct PG_lcd_t *lcd, int idx);

void PG_lcd_select_chip(struct PG_lcd_t *lcd, int chip);
void PG_lcd_unselect_chip(struct PG_lcd_t *lcd);
void PG_lcd_write_data_bit(struct PG_lcd_t *lcd, uint8_t data);


static const int FPS_SAMPLING_SIZE = 100;
struct fps_counter_t {
    struct timespec begin_tspec;
    int sampling_idx;
};
static struct fps_counter_t g_fps_counter;
static void fps_counter_initialize(struct fps_counter_t *counter);
static void fps_counter_update(struct fps_counter_t *counter);

// helper function
struct timespec PG_timespec_subtract(struct timespec *a, struct timespec *b)
{
    struct timespec diff;
    diff.tv_sec = a->tv_sec - b->tv_sec;
    diff.tv_nsec = a->tv_nsec - b->tv_nsec;
    if(diff.tv_nsec < 0) {
        diff.tv_sec -= 1;
        diff.tv_nsec += 1000 * 1000 * 1000;
    }
    assert(diff.tv_sec >= 0);
    assert(diff.tv_nsec >= 0);
    return diff;
}

static void PG_nanosleep(int nsec)
{
    struct timespec dt;
    struct timespec rmtp;
    dt.tv_sec = 0;
    dt.tv_nsec = nsec;
    nanosleep(&dt, &rmtp);
}

void fps_counter_initialize(struct fps_counter_t *counter)
{
    memset(counter, 0, sizeof(*counter));
    counter->sampling_idx = 0;
    clock_gettime(CLOCK_MONOTONIC, &counter->begin_tspec);
}

void fps_counter_update(struct fps_counter_t *counter)
{
    counter->sampling_idx += 1;
    if(counter->sampling_idx < FPS_SAMPLING_SIZE) {
        return;
    }

    struct timespec end_tspec;
    clock_gettime(CLOCK_MONOTONIC, &end_tspec);

    struct timespec diff = PG_timespec_subtract(&end_tspec, &counter->begin_tspec);
    float sec = ((diff.tv_sec) + (diff.tv_nsec / 1000000000.0)) / FPS_SAMPLING_SIZE;
    float fps = 1.0 / sec;
    printf("Avg FPS = %f, Avg delta = %f sec\n", fps, sec);
    fflush(stdout);

    memcpy(&counter->begin_tspec, &end_tspec, sizeof(end_tspec));
    counter->sampling_idx = 0;
}

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
    PG_nanosleep(1);
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
bool PG_lcd_gpio_is_alive(struct PG_lcd_t *lcd)
{
    UNUSED(lcd);
    return true;
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
bool PG_lcd_dummy_is_alive(struct PG_lcd_t *lcd)
{
    UNUSED(lcd);
    return true;
}


// glfw backend
void PG_lcd_glfw_pin_set_val(struct PG_lcd_t *lcd, uint8_t pin, int val)
{
    struct pin_tuple_t {
        uint8_t pin;
        uint8_t* addr;
    };
    struct pin_tuple_t pin_tuple_list[] = {
        { lcd->pin_rs, &lcd->glfw_val_rs },
        { lcd->pin_e, &lcd->glfw_val_e },
        { lcd->pin_cs1, &lcd->glfw_val_cs1 },
        { lcd->pin_cs2, &lcd->glfw_val_cs2 },
        { lcd->pin_rst, &lcd->glfw_val_rst },
        { lcd->pin_led, &lcd->glfw_val_led },
    };
    int count = sizeof(pin_tuple_list) / sizeof(pin_tuple_list[0]);
    for(int i = 0 ; i < count ; ++i) {
        if(pin_tuple_list[i].pin == pin) {
            *pin_tuple_list[i].addr = val;
            return;
        }
    }

    // data bits는 별도 처리
    uint8_t data_pin_list[DATA_PIN_COUNT];
    PG_lcd_fill_data_pin(lcd, data_pin_list);
    for(int i = 0 ; i < 8 ; ++i) {
        if(data_pin_list[i] == pin) {
            uint8_t base_mask = 1 << i;
            if(val) {
                lcd->glfw_val_data_bits |= base_mask;
            } else {
                lcd->glfw_val_data_bits &= ~base_mask;
            }
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

    uint8_t data_bits = lcd->glfw_val_data_bits;

    if(lcd->glfw_val_cs1 == 1) {
        lcd->glfw_state_chip = 0;
    }
    if(lcd->glfw_val_cs2 == 1) {
        lcd->glfw_state_chip = 1;
    }
    //printf("curr chip : %d\n", glfw_state_chip);

    if(lcd->glfw_val_rs == 0) {
        int shift = 0;
        // display on/off
        shift = DATA_BITS_SET_DISPLAY_ENABLE;
        if((data_bits >> shift) == (MASK_SET_DISPLAY_ENABLE >> shift)) {
            lcd->glfw_state_display_enable = ((1 << shift) - 1) & data_bits;
            //printf("display on/off : %d\n", glfw_state_display_enable);
        }

        // set column
        shift = DATA_BITS_SET_COLUMN;
        if((data_bits >> shift) == (MASK_SET_COLUMN >> shift)) {
            lcd->glfw_state_column = ((1 << shift) - 1) & data_bits;
            //printf("set column : %d\n", glfw_state_column);
        }

        // set page
        shift = DATA_BITS_SET_PAGE;
        if((data_bits >> shift) == (MASK_SET_PAGE >> shift)) {
            lcd->glfw_state_page = ((1 << shift) - 1) & data_bits;
            //printf("set page : %d\n", glfw_state_page);
        }

        // set display start line
        shift = DATA_BITS_SET_START_LINE;
        if((data_bits >> shift) == (MASK_SET_START_LINE >> shift)) {
            lcd->glfw_state_start_line = ((1 << shift) - 1) & data_bits;
            //printf("set start line : %d\n", glfw_state_start_line);
        }

    } else {
        // write display data
        //printf("write data\n");
        lcd->glfw_framebuffer.data[lcd->glfw_state_chip][lcd->glfw_state_page][lcd->glfw_state_column] = data_bits;

        int chip_columns = (lcd->columns / lcd->chips);
        lcd->glfw_state_column = (lcd->glfw_state_column + 1) % chip_columns;
    }
}

int PG_lcd_glfw_window_width(struct PG_lcd_t *lcd)
{
    int width = 0;
    width += GLFW_LCD_PIXEL_SIZE * lcd->columns;
    width += GLFW_LCD_BASE_X * 2;
    width += GLFW_LCD_PADDING * (lcd->columns - 1);
    return width;
}

int PG_lcd_glfw_window_height(struct PG_lcd_t *lcd)
{
    int height = 0;
    height += GLFW_LCD_PIXEL_SIZE * lcd->rows;
    height += GLFW_LCD_BASE_Y * 2;
    height += GLFW_LCD_PADDING * (lcd->rows - 1);
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

    lcd->glfw_window = glfwCreateWindow(width, height, "GLFW Backend", NULL, NULL);
    if(!lcd->glfw_window) {
        glfwTerminate();
    }

    glfwMakeContextCurrent(lcd->glfw_window);
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

    if(!lcd->glfw_state_display_enable) {
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
                uint8_t data = lcd->glfw_framebuffer.data[chip][page][column];
                for(int i = 0 ; i < 8 ; ++i) {
                    uint8_t mask = (1 << i);
                    int val = data & mask;
                    if(!val) {
                        continue;
                    }
                    int x = chip * chip_columns + column;
                    int y = page * 8 + i;

                    int l = GLFW_LCD_BASE_X + x * (GLFW_LCD_PIXEL_SIZE + GLFW_LCD_PADDING);
                    int t = GLFW_LCD_BASE_Y + y * (GLFW_LCD_PIXEL_SIZE + GLFW_LCD_PADDING);
                    int r = l + GLFW_LCD_PIXEL_SIZE;
                    int b = t + GLFW_LCD_PIXEL_SIZE;

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

    glfwSwapBuffers(lcd->glfw_window);
    glfwPollEvents();

    return 0;
}
bool PG_lcd_glfw_is_alive(struct PG_lcd_t *lcd)
{
    UNUSED(lcd);
    if(glfwWindowShouldClose(lcd->glfw_window)) {
        return false;
    } else {
        return true;
    }
}

void PG_lcd_fill_all_pin(struct PG_lcd_t *lcd, uint8_t pin_table[PIN_COUNT])
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
void PG_lcd_fill_data_pin(struct PG_lcd_t *lcd, uint8_t pin_table[DATA_PIN_COUNT])
{
    int i = 0 ;
    *(pin_table + i++) = lcd->pin_d0;
    *(pin_table + i++) = lcd->pin_d1;
    *(pin_table + i++) = lcd->pin_d2;
    *(pin_table + i++) = lcd->pin_d3;
    *(pin_table + i++) = lcd->pin_d4;
    *(pin_table + i++) = lcd->pin_d5;
    *(pin_table + i++) = lcd->pin_d6;
    *(pin_table + i++) = lcd->pin_d7;
}

void PG_lcd_reset(struct PG_lcd_t *lcd)
{
    PG_lcd_pin_off(lcd, lcd->pin_rst);
    PG_nanosleep(1);
    PG_lcd_pin_on(lcd, lcd->pin_rst);
}

void PG_lcd_initialize(struct PG_lcd_t *lcd, PG_backend_t backend_type)
{
    memset(lcd, 0, sizeof(*lcd));
    lcd->backend = backend_type;
    // set default value
    lcd->rows = PG_ROWS;
    lcd->columns = PG_COLUMNS;
    lcd->pages = PG_PAGES;
    lcd->chips = PG_CHIPS;

    // for backend
    switch(backend_type) {
        case PG_BACKEND_GPIO:
            lcd->pin_set_val = PG_lcd_gpio_pin_set_val;
            lcd->pulse = PG_lcd_gpio_pulse;
            lcd->setup = PG_lcd_gpio_setup;
            lcd->frame_end_callback = PG_lcd_gpio_frame_end_callback;
            lcd->is_alive = PG_lcd_gpio_is_alive;
            break;
        case PG_BACKEND_GLFW:
            lcd->pin_set_val = PG_lcd_glfw_pin_set_val;
            lcd->pulse = PG_lcd_glfw_pulse;
            lcd->setup = PG_lcd_glfw_setup;
            lcd->frame_end_callback = PG_lcd_glfw_frame_end_callback;
            lcd->is_alive = PG_lcd_glfw_is_alive;
            break;
        case PG_BACKEND_DUMMY:
            lcd->pin_set_val = PG_lcd_dummy_pin_set_val;
            lcd->pulse = PG_lcd_dummy_pulse;
            lcd->setup = PG_lcd_dummy_setup;
            lcd->frame_end_callback = PG_lcd_dummy_frame_end_callback;
            lcd->is_alive = PG_lcd_dummy_is_alive;
            break;
        default:
            assert(!"invalid backend type");
            break;
    }

    fps_counter_initialize(&g_fps_counter);
}

void PG_lcd_destroy(struct PG_lcd_t *lcd)
{
    if(lcd->glfw_window != NULL) {
        glfwDestroyWindow(lcd->glfw_window);
        glfwTerminate();
        lcd->glfw_window = NULL;
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

void PG_lcd_set_page(struct PG_lcd_t *lcd, int page)
{
    uint8_t data = MASK_SET_PAGE | page;
    PG_lcd_write_data_bit(lcd, data);
    lcd->pulse(lcd);
}

void PG_lcd_set_display_enable(struct PG_lcd_t *lcd, int val)
{
    val = val & ((1 << DATA_BITS_SET_DISPLAY_ENABLE) - 1);
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
    idx = idx & ((1 << DATA_BITS_SET_START_LINE) - 1);

    PG_lcd_pin_on(lcd, lcd->pin_cs1);
    PG_lcd_pin_on(lcd, lcd->pin_cs2);

    // 1 1 ? ?  ? ? ? ?
    uint8_t data = MASK_SET_START_LINE | idx;
    PG_lcd_write_data_bit(lcd, data);
    lcd->pulse(lcd);

    PG_lcd_unselect_chip(lcd);
}

void PG_lcd_set_column(struct PG_lcd_t *lcd, int column)
{
    column = column & ((1 << DATA_BITS_SET_COLUMN) - 1);

    // 0 1 ? ? ? ? ? ?
    uint8_t data = MASK_SET_COLUMN | column;
    PG_lcd_write_data_bit(lcd, data);
    lcd->pulse(lcd);
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
    uint8_t data_pin_table[DATA_PIN_COUNT];
    PG_lcd_fill_data_pin(lcd, data_pin_table);

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

// 최대 60 fps로 제한하는 목적
void PG_lcd_render_begin(struct PG_lcd_t *lcd)
{
    clock_gettime(CLOCK_MONOTONIC, &lcd->render_begin_tspec);
}

void PG_lcd_render_end(struct PG_lcd_t *lcd)
{
    struct timespec lcd_render_end_tspec;
    clock_gettime(CLOCK_MONOTONIC, &lcd_render_end_tspec);

    struct timespec diff = PG_timespec_subtract(&lcd_render_end_tspec, &lcd->render_begin_tspec);
    int milli = (diff.tv_sec * 1000) + (diff.tv_nsec / 1000 / 1000);

    const int MAX_FPS = 60;
    int target_milli = (int)(1000.0 / MAX_FPS);
    if(milli < target_milli) {
#ifdef __arm__
        int sleep_milli = target_milli - milli - 1;
        usleep(sleep_milli * 1000);
#else
        usleep(1);
#endif
    }

    // update fps
    fps_counter_update(&g_fps_counter);
}

void PG_lcd_commit_buffer(struct PG_lcd_t *lcd)
{
    PG_lcd_render_begin(lcd);
    for(int chip = 0 ; chip < lcd->chips ; ++chip) {
        PG_lcd_select_chip(lcd, chip);

        for(int page = 0 ; page < lcd->pages ; ++page) {
            PG_lcd_set_page(lcd, page);
            PG_lcd_set_column(lcd, 0);

            for(int column = 0 ; column < (lcd->columns / lcd->chips) ; ++column) {
                PG_lcd_pin_on(lcd, lcd->pin_rs);

                uint8_t data = lcd->buffer.data[chip][page][column];
                PG_lcd_write_data_bit(lcd, data);
                lcd->pulse(lcd);

                PG_lcd_pin_off(lcd, lcd->pin_rs);
            }
        }
        PG_lcd_unselect_chip(lcd);
    }
    lcd->frame_end_callback(lcd);
    PG_lcd_render_end(lcd);
}

void PG_lcd_render_buffer(struct PG_lcd_t *lcd, struct PG_framebuffer_t *buffer)
{
    PG_lcd_render_begin(lcd);

    // diff가 존재하는 page/chip 찾아내기
    // 해당 page/chip에서만 변경을 수행하면 명령을 줄일수 있다
    int diff_list[lcd->pages * lcd->chips];
    memset(diff_list, -1, sizeof(diff_list));

    int chip_column = lcd->columns / lcd->chips;
    int diff_list_idx = 0;
    for(int chip = 0 ; chip < lcd->chips ; ++chip) {
        for(int page = 0 ; page < lcd->pages ; ++page) {
            for(int column = 0 ; column < chip_column ; ++column) {
                uint8_t prev_data = lcd->buffer.data[chip][page][column];
                uint8_t next_data = buffer->data[chip][page][column];
                if(prev_data == next_data) {
                    continue;
                }
                diff_list[diff_list_idx] = chip * lcd->pages + page;
                diff_list_idx++;
                break;
            }
        }
    }

    int latest_chip = -1;
    for(int diff_idx = 0 ; diff_idx < diff_list_idx ; ++diff_idx) {
        int chip = diff_list[diff_idx] / lcd->pages;
        int page = diff_list[diff_idx] % lcd->pages;

        if(latest_chip != chip) {
            PG_lcd_select_chip(lcd, chip);
        }

        PG_lcd_set_page(lcd, page);
        PG_lcd_set_column(lcd, 0);

        int latest_column = -1;
        for(int column = 0 ; column < (lcd->columns / lcd->chips) ; ++column) {
            uint8_t prev_data = lcd->buffer.data[chip][page][column];
            uint8_t next_data = buffer->data[chip][page][column];
            if(prev_data == next_data) {
                continue;
            }

            if(latest_column + 1 != column) {
                PG_lcd_set_column(lcd, column);
                latest_column = column;
            }

            PG_lcd_pin_on(lcd, lcd->pin_rs);

            PG_lcd_write_data_bit(lcd, next_data);
            lcd->pulse(lcd);

            PG_lcd_pin_off(lcd, lcd->pin_rs);
        }

        if(latest_chip != chip) {
            PG_lcd_unselect_chip(lcd);
            latest_chip = chip;
        }
    }
    memcpy(&lcd->buffer, buffer, sizeof(struct PG_framebuffer_t));

    lcd->frame_end_callback(lcd);
    PG_lcd_render_end(lcd);
}

void PG_framebuffer_clear(struct PG_framebuffer_t *buffer)
{
    memset(buffer, 0, sizeof(*buffer));
}

void PG_framebuffer_write_sample_pattern(struct PG_framebuffer_t *buffer)
{
    int chip_columns = PG_COLUMNS / PG_CHIPS;

    for(int page = 0 ; page < PG_PAGES ; ++page) {
        for(int chip = 0 ; chip < PG_CHIPS ; ++chip) {
            for(int column = 0 ; column < chip_columns ; ++column) {
                uint8_t data = (column % 2) ? 0b10101010 : 0b01010101;
                buffer->data[chip][page][column] = data;
            }
        }
    }
}
