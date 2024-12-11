#ifndef COMMON_THINGZ_DISPLAY_RAW_IMG_H_
#define COMMON_THINGZ_DISPLAY_RAW_IMG_H_

#include "py/obj.h"

#include "common-thingz/thingz_screen/thingz_screen_raw.h"

typedef struct _thingz_display_raw_img_obj_t {
    mp_obj_base_t base;
    char* path;
    uint8_t x;
    uint8_t y;
    uint8_t screen_x;
    uint8_t screen_y;
    thingz_screen_bitmap_t bmp;
    uint8_t show;
    uint8_t screen_show;
    uint32_t white_replacement_color;
    uint32_t screen_white_replacement_color;
} thingz_display_raw_img_obj_t;

extern const mp_obj_type_t mp_thingz_display_raw_img_type;

#endif