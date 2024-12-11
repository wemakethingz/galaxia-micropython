#ifndef MICROPY_INCLUDED_ESP32S2_DEBUG_MODE_UI_SELECTOR_H
#define MICROPY_INCLUDED_ESP32S2_DEBUG_MODE_UI_SELECTOR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct debug_mode_ui_selector_t{
    int8_t x;
    int8_t y;
    int8_t x_min;
    int8_t x_max;
    int8_t y_min;
    int8_t y_max;
    bool force_refresh;
    char selector;
    int64_t blink_timestamp;
    int64_t blink_period;
    bool show;
} debug_mode_ui_selector_t;


void debug_mode_ui_selector_init(debug_mode_ui_selector_t* selector, int8_t x_min, int8_t x_max, int8_t y_min, int8_t y_max, char s, int64_t blink_period);
void debug_mode_ui_selector_set_position(debug_mode_ui_selector_t* selector, int8_t x, int8_t y);
void debug_mode_ui_selector_print(debug_mode_ui_selector_t* selector);
int8_t debug_mode_ui_selector_get_x(debug_mode_ui_selector_t* selector);
int8_t debug_mode_ui_selector_get_y(debug_mode_ui_selector_t* selector);


#endif