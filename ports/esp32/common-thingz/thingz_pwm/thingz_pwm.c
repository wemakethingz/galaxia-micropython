#include "thingz_pwm.h"

#include "py/mperrno.h"
#include "py/runtime.h"

#include "driver/ledc.h"

typedef struct {
	bool used;
    ledc_channel_config_t config;
   
} thingz_pwm_config_t;

static thingz_pwm_config_t* thingz_pwm_configs;

void thingz_pwm_construct(thingz_pwm_obj_t* pwm, const mcu_pin_obj_t *pin){
    size_t i;
    for(i = 0; i < 5; i++){
        if(thingz_pwm_configs[i].used == false){
            ledc_timer_config_t ledc_timer = {
                .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
                .freq_hz = 5000,                      // frequency of PWM signal
                .speed_mode = LEDC_LOW_SPEED_MODE,   // timer mode
                .timer_num = LEDC_TIMER_1,            // timer index
                .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
            };

            ledc_timer_config(&ledc_timer);
            
            thingz_pwm_configs[i].config.channel    = 3+i; //0,1,2 are used by LED
            thingz_pwm_configs[i].config.duty       = 0;
            thingz_pwm_configs[i].config.gpio_num   = pin->number;
            thingz_pwm_configs[i].config.speed_mode = LEDC_LOW_SPEED_MODE;
            thingz_pwm_configs[i].config.hpoint     = 0;
            thingz_pwm_configs[i].config.timer_sel  = LEDC_TIMER_1;
            thingz_pwm_configs[i].used = true;

            ledc_channel_config(&thingz_pwm_configs[i].config);
            pwm->index = i;
            return;
        }
    }
    pwm->index = -1;
    mp_raise_RuntimeError(translate("No more PWM available"));
}

void thingz_pwm_deinit(thingz_pwm_obj_t* pwm){
    if(pwm->index >= 0){
        thingz_pwm_configs[pwm->index].config.duty = 0;
        ledc_channel_config(&thingz_pwm_configs[pwm->index].config);
        thingz_pwm_configs[pwm->index].used = false;
    }
}

void thingz_pwm_set_duty(thingz_pwm_obj_t* pwm, uint32_t value){
    if(pwm->index >= 0){
        thingz_pwm_configs[pwm->index].config.duty = value;
        ledc_channel_config(&thingz_pwm_configs[pwm->index].config);
    }
}

bool thingz_pwm_set_period(thingz_pwm_obj_t* pwm, int32_t period_us){
    if(period_us < 0)
        return true;
    if(pwm->index >= 0){
        ledc_timer_config_t ledc_timer = {
            .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
            .freq_hz = 1.0/((float)period_us/1000000.0),                      // frequency of PWM signal
            .speed_mode = LEDC_LOW_SPEED_MODE,   // timer mode
            .timer_num = LEDC_TIMER_1,            // timer index
            .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
        };

        ledc_timer_config(&ledc_timer);
    }
    return false;
}

uint32_t thingz_pwm_get_period(thingz_pwm_obj_t* pwm){
    uint32_t freq = ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1);
    if(freq == 0)
        return 0;
    return (1.0/(float)freq)*1000000;
}

void thingz_pwm_global_init(void){
    size_t i;

    thingz_pwm_configs = (thingz_pwm_config_t*) malloc(8*sizeof(thingz_pwm_config_t)); 
    for(i = 0; i < 5; i++){
        thingz_pwm_configs[i].used = false;
    }
}

void thingz_pwm_global_deinit(void){
    if(thingz_pwm_configs)
        free(thingz_pwm_configs);
}