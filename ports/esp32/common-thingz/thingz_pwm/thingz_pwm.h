#ifndef COMMON_THINGZ_PWM_H_
#define COMMON_THINGZ_PWM_H_

#include "py/obj.h"

#include "pins.h"

extern const mp_obj_type_t thingz_pwm_type;

typedef struct {
	mp_obj_base_t base;
    const mcu_pin_obj_t* pin;
    int8_t index;
} thingz_pwm_obj_t;

void thingz_pwm_construct(thingz_pwm_obj_t* pwm, const mcu_pin_obj_t *pin);
void thingz_pwm_deinit(thingz_pwm_obj_t* pwm);

void thingz_pwm_set_duty(thingz_pwm_obj_t* pwm, uint32_t value);
bool thingz_pwm_set_period(thingz_pwm_obj_t* pwm, int32_t period_us);
uint32_t thingz_pwm_get_period(thingz_pwm_obj_t* pwm);

void thingz_pwm_global_init(void);
void thingz_pwm_global_deinit(void);

#endif