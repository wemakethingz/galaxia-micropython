#ifndef COMMON_THINGZ_DISPLAY_RAW_RECTANGLE_H_
#define COMMON_THINGZ_DISPLAY_RAW_RECTANGLE_H_

#include "py/obj.h"

#include "common-thingz/thingz_screen/thingz_screen_raw.h"

typedef struct _thingz_display_raw_rectangle_obj_t {
    mp_obj_base_t base;
    int16_t x;
    int16_t y;
    int16_t screen_x;
    int16_t screen_y;
    uint8_t screen_height;
    uint8_t screen_width;
    uint8_t height;
    uint8_t width;
    uint8_t show;
    uint8_t screen_show;
    uint32_t color;
    uint32_t screen_color;
} thingz_display_raw_rectangle_obj_t;

extern const mp_obj_type_t mp_thingz_display_raw_rectangle_type;

#endif