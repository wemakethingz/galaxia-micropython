#ifndef COMMON_THINGZ_SCREEN_PLOT_H_
#define COMMON_THINGZ_SCREEN_PLOT_H_

#include <stdint.h>

#include "py/objfun.h"

typedef struct thingz_screen_obj thingz_screen_obj_t;

typedef struct {
    uint8_t next_point_x;
    int32_t scale[2];
    int8_t axis[160];
    thingz_screen_obj_t *screen;
    uint8_t is_shown;
    mp_obj_t animate_cb;
    uint32_t animate_interval;
} thingz_screen_plot_t;

void thingz_screen_plot_init(thingz_screen_plot_t* plot, thingz_screen_obj_t *screen);
void thingz_screen_plot_refresh(thingz_screen_plot_t *plot);
void thingz_screen_plot_clear_column(thingz_screen_plot_t* plot, uint8_t x);
void thingz_screen_plot_add_point(thingz_screen_plot_t *plot, float value);
void thingz_screen_plot_set_animate_function(thingz_screen_plot_t* plot, mp_obj_fun_bc_t* fun, uint32_t interval);
void thingz_screen_plot_set_scale(thingz_screen_plot_t* plot, int32_t* scale);

void thingz_screen_plot_enter(thingz_screen_plot_t *plot);
void thingz_screen_plot_exit(thingz_screen_plot_t *plot);

#endif