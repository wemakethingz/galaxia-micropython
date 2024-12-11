#ifndef COMMON_THINGZ_BUTTON_H_
#define COMMON_THINGZ_BUTTON_H_

#include "py/obj.h"

#include "esp_timer.h"

extern const mp_obj_type_t thingz_button_type;

typedef struct {
	mp_obj_base_t base;
    uint8_t pin;
    bool state;
    bool was_pressed;
    uint32_t presses_count;
    int64_t lastInterrupt;
    esp_timer_create_args_t debounce_timer_args;
    esp_timer_handle_t debounce_timer;
    mp_obj_t pressed_callback;
    size_t pressed_callback_args_num;
} thingz_button_obj_t;


void thingz_button_init(thingz_button_obj_t* button, uint8_t pin);
#endif