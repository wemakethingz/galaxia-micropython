#ifndef COMMON_THINGZ_SCREEN_DEBUG_H_
#define COMMON_THINGZ_SCREEN_DEBUG_H_

#include "stdint.h"

#include "mpconfigboard.h"

#include "py/obj.h"


typedef struct thingz_screen_obj thingz_screen_obj_t;

typedef struct{
    int16_t cursor_x;
    int16_t cursor_y;
    int16_t last_cursor_x;
    int16_t last_cursor_y;
    int16_t virtual_top;

    uint8_t data[MICROPY_THING_SCREEN_BUFF_HEIGHT_LEN][MICROPY_THING_SCREEN_BUFF_WIDTH_LEN];
    uint16_t dataLines;
    uint16_t dataColumns;

    thingz_screen_obj_t *screen;
} thingz_screen_debug_t;

void thingz_screen_debug_init(thingz_screen_debug_t *debug, thingz_screen_obj_t *screen);
void thingz_screen_debug_refresh(thingz_screen_debug_t *debug);

void thingz_screen_debug_set_str(thingz_screen_debug_t *debug, uint8_t* str, uint8_t x, uint8_t y);

void thingz_screen_repl_enter();
void thingz_screen_repl_exit();

#endif