#ifndef COMMON_THINGZ_DISPLAY_PLOT_H_
#define COMMON_THINGZ_DISPLAY_PLOT_H_

#include "py/obj.h"

extern const mp_obj_type_t thingz_display_plot_type;

typedef struct {
	mp_obj_base_t base;

} thingz_display_plot_obj_t;


void thingz_display_plot_init(thingz_display_plot_obj_t* plot);
void thingz_display_plot_deinit(thingz_display_plot_obj_t* plot);


#endif
