#include "thingz_led.h"

#include "py/runtime.h"
#include "py/objstr.h"
#include "py/obj.h"

#include "driver/gpio.h"
#include "driver/adc.h"

#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// static pwmio_pwmout_obj_t led_circuitpy[3];
// static machine_pwm_obj_t led_micropy[3];

static void thingz_led_stop_pwm(thingz_led_obj_t* led){
    int ch;
    for(ch = 0; ch < 3; ch++){
        ledc_stop(LEDC_LOW_SPEED_MODE, led->pwm_channel[ch].channel, 0);
    }
}

static void thingz_led_config_pwm(thingz_led_obj_t* led, uint16_t r_duty, uint16_t g_duty, uint16_t b_duty){
    
    // ledc_timer_config_t ledc_timer = {
    //     .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
    //     .freq_hz = 2000,                      // frequency of PWM signal
    //     .speed_mode = LEDC_LOW_SPEED_MODE,           // timer mode
    //     .timer_num = LEDC_TIMER_1,            // timer index
    //     .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    // };
    uint8_t ch;

    // ledc_timer_config(&ledc_timer);
    
    led->pwm_channel[0].channel    = 7;//led_circuitpy[0].chan_handle.channel;
    led->pwm_channel[0].duty       = r_duty;
    led->pwm_channel[0].gpio_num   = led->pinR;
    led->pwm_channel[0].speed_mode = LEDC_LOW_SPEED_MODE;
    led->pwm_channel[0].hpoint     = 0;
    led->pwm_channel[0].timer_sel  = LEDC_TIMER_0;

    led->pwm_channel[1].channel    = 6;//led_circuitpy[1].chan_handle.channel;
    led->pwm_channel[1].duty       = g_duty;
    led->pwm_channel[1].gpio_num   = led->pinG;
    led->pwm_channel[1].speed_mode = LEDC_LOW_SPEED_MODE;
    led->pwm_channel[1].hpoint     = 0;
    led->pwm_channel[1].timer_sel  = LEDC_TIMER_0;
        
    led->pwm_channel[2].channel    = 5;//led_circuitpy[2].chan_handle.channel;
    led->pwm_channel[2].duty       = b_duty;
    led->pwm_channel[2].gpio_num   = led->pinB;
    led->pwm_channel[2].speed_mode = LEDC_LOW_SPEED_MODE;
    led->pwm_channel[2].hpoint     = 0;
    led->pwm_channel[2].timer_sel  = LEDC_TIMER_0;

    for (ch = 0; ch < 3; ch++) {
        ledc_channel_config(&led->pwm_channel[ch]);
    }

}

void thingz_led_init(thingz_led_obj_t* led, int8_t pinR, int8_t pinG, int8_t pinB,  int8_t pinGND){

    led->pinR = pinR;
    led->pinG = pinG;
    led->pinB = pinB;
    led->pinGND = pinGND;

    led->base.type = &thingz_led_type;

    #if defined(DEBUG) || defined(ENABLE_JTAG)
        //JTAG share pins with led
        return;
    #endif

    // Step 1: Reset all pins first to clear any previous configuration
    gpio_reset_pin(pinR);
    gpio_reset_pin(pinG);
    gpio_reset_pin(pinB);
    gpio_reset_pin(pinGND);

    // Step 2: Configure GND pin as output (drive low) with fully initialized struct
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << pinGND,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(pinGND, 0);


    // Step 3: Create PWM objects for RGB pins (after reset)
    mp_obj_t args[2];
    args[0] = mp_obj_new_int(pinR);
    args[1] = mp_obj_new_int(2000);
    args[0] = MP_OBJ_TYPE_GET_SLOT(&machine_pin_type, make_new)(&machine_pin_type, 1, 0, args);
    led->mp_pwm[0] = MP_OBJ_TYPE_GET_SLOT(&machine_pwm_type, make_new)(&machine_pwm_type, 2, 0, args);

    args[0] = mp_obj_new_int(pinG);
    args[0] = MP_OBJ_TYPE_GET_SLOT(&machine_pin_type, make_new)(&machine_pin_type, 1, 0, args);
    led->mp_pwm[1] = MP_OBJ_TYPE_GET_SLOT(&machine_pwm_type, make_new)(&machine_pwm_type, 2, 0, args);

    args[0] = mp_obj_new_int(pinB);
    args[0] = MP_OBJ_TYPE_GET_SLOT(&machine_pin_type, make_new)(&machine_pin_type, 1, 0, args);
    led->mp_pwm[2] = MP_OBJ_TYPE_GET_SLOT(&machine_pwm_type, make_new)(&machine_pwm_type, 2, 0, args);

    // Step 4: Configure PWM with initial duty cycle of 0 (LED off)
    thingz_led_config_pwm(led, 0, 0, 0);

}


static void thingz_led_set_colors(thingz_led_obj_t* led, uint32_t r, uint32_t g, uint32_t b){

    #if defined(DEBUG) || defined(ENABLE_JTAG)
        //JTAG share pins with led
        return;
    #endif
    // gpio_config_t io_conf;
    uint16_t r_duty, g_duty, b_duty;
    // mp_printf(MP_PYTHON_PRINTER, "R: %d, G: %d, B: %d\n", r, g, b);
    // io_conf.pin_bit_mask = 1ull << led->pinR->number | 1ull << led->pinG->number | 1ull << led->pinB->number | 1ull << led->pinGND->number;
    // io_conf.mode = GPIO_MODE_INPUT_OUTPUT;

    // gpio_reset_pin(led->pinR->number);
    // gpio_reset_pin(led->pinG->number);
    // gpio_reset_pin(led->pinB->number);
    // gpio_reset_pin(led->pinGND->number);

    // gpio_config(&io_conf);

    // gpio_set_level(led->pinGND->number, 0);    

    if(r > 255)
        r = 255;
    if(g > 255)
        g = 255;
    if(b > 255)
        b = 255;

    led->rgb_values[0] = r;
    led->rgb_values[1] = g;
    led->rgb_values[2] = b;

    r_duty = 8192.0 * ((float)r/255.0);
    g_duty = 8192.0 * ((float)g/255.0);
    b_duty = 8192.0 * ((float)b/255.0);

    // if(r_duty != led->pwm_channel[0].duty || g_duty != led->pwm_channel[0].duty || b_duty != led->pwm_channel[0].duty){
        led->pwm_channel[0].duty = r_duty;
        led->pwm_channel[1].duty = g_duty;
        led->pwm_channel[2].duty = b_duty;

        thingz_led_config_pwm(led, r_duty, g_duty, b_duty);

    // }
}

static uint32_t thingz_led_read_light_pin(thingz_led_obj_t* led, uint32_t pin){
    #if defined(DEBUG) || defined(ENABLE_JTAG)
        //JTAG share pins with led
        return 0;
    #endif
    gpio_reset_pin(led->pinGND);
    gpio_reset_pin(led->pinR);
    gpio_reset_pin(led->pinG);
    gpio_reset_pin(led->pinB);
    
    gpio_set_direction(led->pinGND, GPIO_MODE_INPUT);
    gpio_set_direction(led->pinR, GPIO_MODE_INPUT);
    gpio_set_direction(led->pinG, GPIO_MODE_INPUT);
    gpio_set_direction(led->pinB, GPIO_MODE_INPUT);

    gpio_set_pull_mode(led->pinGND, GPIO_FLOATING);
    gpio_set_pull_mode(led->pinR, GPIO_PULLDOWN_ONLY);
    gpio_set_pull_mode(led->pinG, GPIO_PULLDOWN_ONLY);
    gpio_set_pull_mode(led->pinB, GPIO_PULLDOWN_ONLY);

    uint32_t start = esp_timer_get_time();
    while(esp_timer_get_time()-start < 50){
    }

    gpio_set_direction(led->pinGND, GPIO_MODE_OUTPUT);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(led->pinGND, 0);
    gpio_set_level(pin, 1);

    start = esp_timer_get_time();
    while(esp_timer_get_time()-start < 5){
    }
    gpio_set_level(led->pinGND, 1);
    gpio_set_level(pin, 0);
    start = esp_timer_get_time();

    while(esp_timer_get_time()-start < 2500){
    }

    start = esp_timer_get_time();
    gpio_set_direction(led->pinGND, GPIO_MODE_INPUT);
    
    adc1_config_width(ADC_WIDTH_BIT_13);
    adc1_config_channel_atten(ADC1_CHANNEL_9,ADC_ATTEN_DB_11);

    int val = adc1_get_raw(ADC1_CHANNEL_9);
    while(val > 5 && esp_timer_get_time()-start < 8400){
        val = adc1_get_raw(ADC1_CHANNEL_9);
    }
    uint32_t result = esp_timer_get_time()-start;
    if(result < 500)
        result = 500;
    else if(result > 8400)
        result = 8400;

    return result;
}

static uint32_t thingz_led_read_light(thingz_led_obj_t* led){
    #if defined(DEBUG) || defined(ENABLE_JTAG)
        //JTAG share pins with led
        return 0;
    #endif
    thingz_led_stop_pwm(led);
    
    uint32_t result = thingz_led_read_light_pin(led, led->pinR);
    uint8_t i;
    for(i=0; i < 9; i++){
        result = (result + thingz_led_read_light_pin(led, led->pinR)) / 2;
    }

    float r = ((float)result - 500.0) * (255.0) / (8400.0 - 500.0);
    r = 255.0 - r;

    gpio_reset_pin(led->pinGND);
    gpio_reset_pin(led->pinR);
    gpio_reset_pin(led->pinG);
    gpio_reset_pin(led->pinB);

    gpio_set_direction(led->pinGND, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(led->pinGND, 0); 

    thingz_led_config_pwm(led, led->pwm_channel[0].duty, led->pwm_channel[1].duty, led->pwm_channel[2].duty);

    return (uint32_t)r;
}
//|
//|
//| """ Thingz LED
//| """
//|
//| class Led:
//|    """Control Galaxia's RGB LED"""
//|
//|

//NEW
static mp_obj_t mp_thingz_led_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_led_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_led_obj_t));
    
    self->base.type = &thingz_led_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_led_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_led_del_obj, mp_thingz_led_del);

//SET COLORS
//|    def set_colors(self, red: int, green: int, blue: int) -> None:
//|        """Set red, green and blue values
//|
//|        :param int red: The red value between 0 and 255
//|        :param int green: The green value between 0 and 255
//|        :param int blue: The blue value between 0 and 255""" 
//|        ...
//|

//SET COLORS
static const mp_arg_t mp_thingz_led_set_colors_args[] = {
    { MP_QSTR_red,      MP_ARG_REQUIRED | MP_ARG_INT, {.u_obj = mp_const_none}},
    { MP_QSTR_green,    MP_ARG_REQUIRED | MP_ARG_INT, {.u_obj = mp_const_none}},
    { MP_QSTR_blue,     MP_ARG_REQUIRED | MP_ARG_INT, {.u_obj = mp_const_none}},
};
#define MP_THINGZ_LED_SET_COLORS_NUM_ARGS MP_ARRAY_SIZE(mp_thingz_led_set_colors_args)

static mp_obj_t mp_thingz_led_set_colors(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    //  return mp_const_none;
    thingz_led_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint32_t red, green, blue;

    // parse args
    mp_arg_val_t vals[MP_THINGZ_LED_SET_COLORS_NUM_ARGS];
    mp_arg_parse_all(n_args - 1, args + 1, kw_args, MP_THINGZ_LED_SET_COLORS_NUM_ARGS, mp_thingz_led_set_colors_args, vals);

    int color =  vals[0].u_int;
    red = color < 0 ? 0 : color;
    color =  vals[1].u_int;
    green = color < 0 ? 0 : color;
    color =  vals[2].u_int;
    blue = color < 0 ? 0 : color;
    thingz_led_set_colors(self, red, green, blue);

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_led_set_colors_obj, 3, mp_thingz_led_set_colors);

//|    def set_red(self, red: int) -> None:
//|        """Set red value
//|
//|        :param int red: The red value between 0 and 255""" 
//|        ...
//|
//SET RED
static const mp_arg_t mp_thingz_led_set_red_args[] = {
    { MP_QSTR_red,      MP_ARG_REQUIRED | MP_ARG_INT, {.u_obj = mp_const_none}},
};
#define MP_THINGZ_LED_SET_RED_NUM_ARGS MP_ARRAY_SIZE(mp_thingz_led_set_red_args)

static mp_obj_t mp_thingz_led_set_red(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    uint32_t red;
    thingz_led_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    // parse args
    mp_arg_val_t vals[MP_THINGZ_LED_SET_RED_NUM_ARGS];
    mp_arg_parse_all(n_args - 1, args + 1, kw_args, MP_THINGZ_LED_SET_RED_NUM_ARGS, mp_thingz_led_set_red_args, vals);
    
    int color =  vals[0].u_int;
    red = color < 0 ? 0 : color;
    thingz_led_set_colors(self, red, self->rgb_values[1], self->rgb_values[2]);

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_led_set_red_obj, 1, mp_thingz_led_set_red);

//|    def set_green(self, green: int) -> None:
//|        """Set green value
//|
//|        :param int green: The green value between 0 and 255""" 
//|        ...
//|
//SET GREEN
static const mp_arg_t mp_thingz_led_set_green_args[] = {
    { MP_QSTR_green,      MP_ARG_REQUIRED | MP_ARG_INT, {.u_obj = mp_const_none}},
};
#define MP_THINGZ_LED_SET_GREEN_NUM_ARGS MP_ARRAY_SIZE(mp_thingz_led_set_green_args)

static mp_obj_t mp_thingz_led_set_green(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    uint32_t green;
    thingz_led_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    // parse args
    mp_arg_val_t vals[MP_THINGZ_LED_SET_GREEN_NUM_ARGS];
    mp_arg_parse_all(n_args - 1, args + 1, kw_args, MP_THINGZ_LED_SET_GREEN_NUM_ARGS, mp_thingz_led_set_green_args, vals);

    int color =  vals[0].u_int;
    green = color < 0 ? 0 : color;
    thingz_led_set_colors(self, self->rgb_values[0], green, self->rgb_values[2]);

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_led_set_green_obj, 1, mp_thingz_led_set_green);

//SET BLUE
//|    def set_blue(self, blue: int) -> None:
//|        """Set blue value
//|
//|        :param int blue: The blue value between 0 and 255""" 
//|        ...
//|
static const mp_arg_t mp_thingz_led_set_blue_args[] = {
    { MP_QSTR_blue,      MP_ARG_REQUIRED | MP_ARG_INT, {.u_obj = mp_const_none}},
};
#define MP_THINGZ_LED_SET_BLUE_NUM_ARGS MP_ARRAY_SIZE(mp_thingz_led_set_blue_args)

static mp_obj_t mp_thingz_led_set_blue(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    uint32_t blue;
    thingz_led_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    // parse args
    mp_arg_val_t vals[MP_THINGZ_LED_SET_BLUE_NUM_ARGS];
    mp_arg_parse_all(n_args - 1, args + 1, kw_args, MP_THINGZ_LED_SET_BLUE_NUM_ARGS, mp_thingz_led_set_blue_args, vals);

    int color =  vals[0].u_int;
    blue = color < 0 ? 0 : color;
    thingz_led_set_colors(self, self->rgb_values[0], self->rgb_values[1], blue);

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_led_set_blue_obj, 1, mp_thingz_led_set_blue);

//GET RED
//|    def get_red(self) -> int:
//|        """
//|        Get red value
//|
//|        :return: The red value between 0 and 255
//|        :rtype: int
//|        """ 
//|        ...
//|
static mp_obj_t mp_thingz_led_get_red(mp_obj_t self_in) {
	thingz_led_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
	return mp_obj_new_int(self->rgb_values[0]);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_led_get_red_obj, mp_thingz_led_get_red);


//GET GREEN
//|    def get_green(self) -> int:
//|        """
//|        Get green value
//|
//|        :return: The green value between 0 and 255
//|        :rtype: int
//|        """ 
//|        ...
//|
static mp_obj_t mp_thingz_led_get_green(mp_obj_t self_in) {
	thingz_led_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
	return mp_obj_new_int(self->rgb_values[1]);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_led_get_green_obj, mp_thingz_led_get_green);

//GET BLUE
//|    def get_blue(self) -> int:
//|        """
//|        Get blue value
//|
//|        :return: The blue value between 0 and 255
//|        :rtype: int
//|        """ 
//|        ...
//|
static mp_obj_t mp_thingz_led_get_blue(mp_obj_t self_in) {
	thingz_led_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
	return mp_obj_new_int(self->rgb_values[2]);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_led_get_blue_obj, mp_thingz_led_get_blue);

//READ_LIGHT_LEVEL
//|    def read_light_level(self) -> int:
//|        """
//|        Get the current light level
//|
//|        :return: The light level between 0 (dark) and 100 (luminous)
//|        :rtype: int
//|        """ 
//|        ...
//|
static mp_obj_t mp_thingz_led_read_light(mp_obj_t self_in) {
    // return mp_obj_new_int(0);
	thingz_led_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint32_t result = thingz_led_read_light(self);
	return mp_obj_new_int(result);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_led_read_light_obj, mp_thingz_led_read_light);

static const mp_map_elem_t thingz_led_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__),     (mp_obj_t)&mp_thingz_led_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_colors),  (mp_obj_t)&mp_thingz_led_set_colors_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_red),     (mp_obj_t)&mp_thingz_led_set_red_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_green),   (mp_obj_t)&mp_thingz_led_set_green_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_blue),    (mp_obj_t)&mp_thingz_led_set_blue_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_red),     (mp_obj_t)&mp_thingz_led_get_red_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_green),   (mp_obj_t)&mp_thingz_led_get_green_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_blue),    (mp_obj_t)&mp_thingz_led_get_blue_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_read_light_level),  (mp_obj_t)&mp_thingz_led_read_light_obj },
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_led_local_dict,
	thingz_led_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_led_type,
    MP_QSTR_Led,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_led_make_new,
    locals_dict, &mp_thingz_led_local_dict
);