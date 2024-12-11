#ifndef MICROPY_INCLUDED_ESP32S2_DEBUG_MODE_UI_SCROLL_H
#define MICROPY_INCLUDED_ESP32S2_DEBUG_MODE_UI_SCROLL_H

#include <stdint.h>
#include <stdbool.h>

#include "debug_mode/ui/text_scroll/text_scroll.h"

struct debug_mode_ui_scroll_t;

typedef bool (*scroll_update_t)(struct debug_mode_ui_scroll_t*);

typedef struct debug_mode_ui_scroll_t{
    uint8_t x;
    uint8_t y_start;
    uint8_t y_end;
    int32_t items_length;
    scroll_update_t update;
    int32_t index;
    void* data;
    uint32_t data_item_length;
    int8_t active_row;
    int8_t last_active_row;
    bool need_update;
    bool first_decorator_char;
    debug_mode_ui_text_scroll_t* text_scrolls;
    bool dynamic_data;
} debug_mode_ui_scroll_t;


void debug_mode_ui_scroll_init(debug_mode_ui_scroll_t* scroll, uint8_t x, uint8_t y_start, uint8_t y_end, int32_t items_length, int32_t index, void* data, uint32_t data_item_length, bool dynamic_data);
void debug_mode_ui_scrool_free(debug_mode_ui_scroll_t* scroll);
int8_t debug_mode_ui_scroll_up(debug_mode_ui_scroll_t* scroll);
int8_t debug_mode_ui_scroll_down(debug_mode_ui_scroll_t* scroll);
void debug_mode_ui_scroll_print(debug_mode_ui_scroll_t* scroll);
char* debug_mode_ui_scroll_get_current_item(debug_mode_ui_scroll_t* scroll);
int8_t debug_mode_ui_scroll_get_current_item_index(debug_mode_ui_scroll_t* scroll);
void debug_mode_ui_scroll_set_current_item_index(debug_mode_ui_scroll_t* scroll, int8_t index);


#endif