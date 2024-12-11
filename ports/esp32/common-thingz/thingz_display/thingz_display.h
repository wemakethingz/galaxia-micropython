#ifndef COMMON_THINGZ_DISPLAY_H_
#define COMMON_THINGZ_DISPLAY_H_

#include "py/obj.h"

extern const mp_obj_type_t thingz_display_type;

typedef struct {
	mp_obj_base_t base;
    
} thingz_display_obj_t;


void thingz_display_init(thingz_display_obj_t* display);
void thingz_display_deinit(thingz_display_obj_t* display);
#endif