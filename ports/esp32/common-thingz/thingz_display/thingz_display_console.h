#ifndef COMMON_THINGZ_DISPLAY_CONSOLE_H_
#define COMMON_THINGZ_DISPLAY_CONSOLE_H_

#include "py/obj.h"

extern const mp_obj_type_t thingz_display_console_type;

typedef struct {
	mp_obj_base_t base;
    
} thingz_display_console_obj_t;


void thingz_display_console_init(thingz_display_console_obj_t* console);
void thingz_display_console_deinit(thingz_display_console_obj_t* console);


#endif
