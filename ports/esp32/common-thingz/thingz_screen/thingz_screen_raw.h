#ifndef COMMON_THINGZ_SCREEN_RAW_H_
#define COMMON_THINGZ_SCREEN_RAW_H_

#include "stdint.h"

#include "mpconfigboard.h"

#include "extmod/vfs_fat.h"

#include "py/obj.h"

#include "freertos/queue.h"


typedef struct thingz_screen_obj thingz_screen_obj_t;

typedef struct _thingz_screen_raw_show_obj_t {
	mp_obj_t show_obj;
    uint8_t was_updated;
	struct _thingz_screen_raw_show_obj_t* prev;
	struct _thingz_screen_raw_show_obj_t* next;
} thingz_screen_raw_show_obj_t;

typedef struct{
    // int16_t cursor_x;
    // int16_t cursor_y;
    // int16_t last_cursor_x;
    // int16_t last_cursor_y;
    // int16_t virtual_top;

    // uint8_t data[MICROPY_THING_SCREEN_BUFF_HEIGHT_LEN-1][MICROPY_THING_SCREEN_BUFF_WIDTH_LEN];
    // uint16_t dataLines;
    // uint16_t dataColumns;
    // uint8_t offsetx;
    // uint8_t offsety;

    thingz_screen_obj_t *screen;
    thingz_screen_raw_show_obj_t* head;
    QueueHandle_t transfer_queue;
} thingz_screen_raw_t;

typedef struct {
    uint8_t shown;
    uint16_t width;
    uint16_t height;
    uint16_t data_offset;
    uint16_t stride;
    uint32_t r_bitmask;
    uint32_t g_bitmask;
    uint32_t b_bitmask;
    uint32_t* palette;
    bool bitfield_compressed;
    uint8_t bits_per_pixel;
    pyb_file_obj_t* file;
    uint32_t white_replacement_color;
} thingz_screen_bitmap_t;

void thingz_screen_raw_init(thingz_screen_raw_t *raw, thingz_screen_obj_t *screen);
void thingz_screen_raw_refresh(thingz_screen_raw_t *raw);
mp_uint_t thingz_screen_raw_write(thingz_screen_raw_t *raw, int16_t x, int16_t y, const void *buf, mp_uint_t size, uint32_t color);
thingz_screen_bitmap_t thingz_screen_raw_print_bmp(thingz_screen_raw_t *raw, int16_t x, int16_t y, const char *file, uint32_t white_replacement_color, uint8_t show);
void thingz_screen_raw_fill_rect(thingz_screen_raw_t* raw, int16_t x, int16_t x2, int16_t y, int16_t y2, uint16_t color);

void thingz_screen_raw_enter(thingz_screen_raw_t *raw);
void thingz_screen_raw_exit(thingz_screen_raw_t *raw);

void thingz_screen_raw_add_show_obj(thingz_screen_raw_t* raw, mp_obj_t obj);
void thingz_screen_raw_remove_show_obj(thingz_screen_raw_t* raw, mp_obj_t obj);

#endif