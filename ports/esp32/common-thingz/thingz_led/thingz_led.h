#ifndef COMMON_THINGZ_LED_H_
#define COMMON_THINGZ_LED_H_

#include "py/obj.h"
#include "extmod/modmachine.h"
#include "modmachine.h"

#include "driver/ledc.h"

extern const mp_obj_type_t thingz_led_type;

typedef struct {
	mp_obj_base_t base;
    int8_t pinR;
    int8_t pinG;
    int8_t pinB;
    int8_t pinGND;
    ledc_channel_config_t pwm_channel[3];
    machine_pwm_obj_t* mp_pwm[3];
    uint32_t rgb_values[3];
} thingz_led_obj_t;


void thingz_led_init(thingz_led_obj_t* led, int8_t pinR, int8_t pinG, int8_t pinB, int8_t pinGND);
#endif