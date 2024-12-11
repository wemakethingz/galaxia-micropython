#ifndef MICROPY_INCLUDED_ESP32S2_DEBUG_MODE_UI_TEXT_SCROLL_H
#define MICROPY_INCLUDED_ESP32S2_DEBUG_MODE_UI_TEXT_SCROLL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct debug_mode_ui_text_scroll_t{
    int8_t x;
    int8_t y;
    int8_t visible_length;
    int32_t length;
    uint32_t current_index;
    char* text;
    int64_t scroll_timestamp;
    int64_t scroll_period;
    bool center;
} debug_mode_ui_text_scroll_t;


void debug_mode_ui_text_scroll_init(debug_mode_ui_text_scroll_t* scroll, int8_t x, int8_t y, int8_t visible_length, char* text, uint32_t length, int64_t scroll_period);
char* debug_mode_ui_text_scroll_get_text(debug_mode_ui_text_scroll_t* scroll);
void debug_mode_ui_text_scroll_set_text(debug_mode_ui_text_scroll_t* scroll, char* text, uint32_t length);
void debug_mode_ui_text_scroll_set_position(debug_mode_ui_text_scroll_t* scroll, int8_t x, int8_t y);
void debug_mode_ui_text_scroll_print(debug_mode_ui_text_scroll_t* scroll);

#endif