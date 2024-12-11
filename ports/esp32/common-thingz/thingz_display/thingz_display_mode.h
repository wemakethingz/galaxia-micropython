#ifndef COMMON_THINGZ_DISPLAY_MODE_H_
#define COMMON_THINGZ_DISPLAY_MODE_H_

#include "py/obj.h"

typedef enum {
    DISPLAY_MODE_CONSOLE,
    DISPLAY_MODE_PLOT
} thingz_display_mode_t;

typedef struct {
    mp_obj_base_t base;
} thingz_display_mode_obj_t;

extern const mp_obj_type_t thingz_display_mode_type;

extern const thingz_display_mode_obj_t thingz_display_mode_console_obj;
extern const thingz_display_mode_obj_t thingz_display_mode_plot_obj;

#endif
