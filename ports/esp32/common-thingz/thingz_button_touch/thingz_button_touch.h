#ifndef COMMON_THINGZ_BUTTON_TOUCH_H_
#define COMMON_THINGZ_BUTTON_TOUCH_H_

#include "py/obj.h"
#include "esp_timer.h"

extern const mp_obj_type_t thingz_button_touch_type;

typedef struct {
	mp_obj_base_t base;
    uint8_t pin;
    bool init;
    bool state;
    bool was_touched;
    uint32_t touches_count;
    uint32_t thresh;
    uint32_t first_thresh;
    mp_obj_t touched_callback;
    size_t touched_callback_num_args; 
    int64_t timestamp;
    esp_timer_create_args_t touch_debounce_timer_args;
    esp_timer_handle_t touch_debounce_timer;
} thingz_button_touch_obj_t;

void thingz_button_touch_common_init(void);
void thingz_button_touch_common_deinit(void);
void thingz_button_touch_init(thingz_button_touch_obj_t* button_touch, uint8_t pin);
void thingz_button_touch_deinit(thingz_button_touch_obj_t* button_touch);
#endif