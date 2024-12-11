#ifndef COMMON_THINGZ_SCREEN_REPL_H_
#define COMMON_THINGZ_SCREEN_REPL_H_

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

    uint8_t data[MICROPY_THING_SCREEN_BUFF_HEIGHT_LEN-1][MICROPY_THING_SCREEN_BUFF_WIDTH_LEN];
    uint16_t dataLines;
    uint16_t dataColumns;
    uint8_t offsetx;
    uint8_t offsety;

    thingz_screen_obj_t *screen;
} thingz_screen_repl_t;

void thingz_screen_repl_init(thingz_screen_repl_t *repl, thingz_screen_obj_t *screen);
void thingz_screen_repl_refresh(thingz_screen_repl_t *repl);
mp_uint_t thingz_screen_repl_write(thingz_screen_repl_t *repl, const void *buf, mp_uint_t size, int *errcode);
void thingz_screen_repl_enter();
void thingz_screen_repl_exit();

#endif