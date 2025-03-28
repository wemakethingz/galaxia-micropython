#ifndef COMMON_THINGZ_DISPLAY_RAW_H_
#define COMMON_THINGZ_DISPLAY_RAW_H_

#include "py/obj.h"

extern const mp_obj_type_t thingz_display_raw_type;


typedef struct {
	mp_obj_base_t base;
} thingz_display_raw_obj_t;



void thingz_display_raw_init(thingz_display_raw_obj_t* raw);
void thingz_display_raw_deinit(thingz_display_raw_obj_t* raw);


#endif
