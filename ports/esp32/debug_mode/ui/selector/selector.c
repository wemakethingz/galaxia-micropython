#include "selector.h"

#include "esp_timer.h"

#include "debug_mode/debug_mode.h"


void debug_mode_ui_selector_init(debug_mode_ui_selector_t* selector, int8_t x_min, int8_t x_max, int8_t y_min, int8_t y_max, char s, int64_t blink_period){
    selector->x = x_min;
    selector->y = y_min;
    selector->x_min = x_min;
    selector->x_max = x_max;
    selector->y_min = y_min;
    selector->y_max = y_max;
    selector->selector = s;
    selector->blink_period = blink_period;
    selector->force_refresh = true;

}

void debug_mode_ui_selector_set_position(debug_mode_ui_selector_t* selector, int8_t x, int8_t y){
    if(x != selector->x || y != selector->y)
        selector->force_refresh = true;
    selector->x = x;
    selector->y = y;

    if(selector->x > selector->x_max){
        selector->x = selector->x_max;
    }else if(selector->x < selector->x_min){
        selector->x = selector->x_min;
    }

    if(selector->y > selector->y_max){
        selector->y = selector->y_max;
    }else if(selector->y < selector->y_min){
        selector->y = selector->y_min;
    }
}

void debug_mode_ui_selector_print(debug_mode_ui_selector_t* selector){
    int64_t t = esp_timer_get_time();
    if(selector->force_refresh || t - selector->blink_timestamp >= selector->blink_period){
        selector->show = !selector->show;
        selector->blink_timestamp = t;
        
        if(selector->force_refresh)
            selector->show = true;

        selector->force_refresh = false;
    }
    if(selector->show){
        char str[2];
        str[1] = 0;
        str[0] = selector->selector;
    
        debug_mode_print_str(selector->x, selector->y, str, 0, 0xffffff);
    }
}

int8_t debug_mode_ui_selector_get_x(debug_mode_ui_selector_t* selector){
    return selector->x;
}

int8_t debug_mode_ui_selector_get_y(debug_mode_ui_selector_t* selector){
    return selector->y;
}