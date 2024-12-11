#include "thingz_microphone.h"

#include "py/runtime.h"
#include "py/objstr.h"

#include "common-hal/microcontroller/Pin.h"
#include "shared-bindings/pwmio/PWMOut.h"

#include "driver/gpio.h"
#include "driver/adc.h"

#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void thingz_microphone_timer_callback(void *args);

static esp_timer_handle_t mic_timer;
static esp_timer_create_args_t mic_timer_args = { .callback = &thingz_microphone_timer_callback, .name = "mic" };

static uint8_t thingz_read_jack_mic(void){
    int val;
    uint8_t retry = 0;

    while(retry < 5){
        esp_err_t err = adc2_get_raw(ADC2_CHANNEL_5, ADC_WIDTH_BIT_13, &val);
        if(err == ESP_OK)
            break;
    }

    if(retry == 5)
        return 0;

    uint8_t value = (val) * (255) / (8191);
    return value;
}

static void thingz_microphone_history_push_event(thingz_microphone_obj_t *microphone, uint8_t event){
    int index = microphone->sound_event_history_index == microphone->sound_event_history_length - 1 ? microphone->sound_event_history_index : microphone->sound_event_history_index - 1;
    if(index == -1 || microphone->sound_event_history[index] != event){
        if(index == microphone->sound_event_history_length - 1){
            int i;
            for(i = 1; i < microphone->sound_event_history_length; i++){
                microphone->sound_event_history[i-1] = microphone->sound_event_history[i];
            }
            microphone->sound_event_history[index] = event;
        }else{
            microphone->sound_event_history[microphone->sound_event_history_index] = event;
            microphone->sound_event_history_index++;
            if(microphone->sound_event_history_index == microphone->sound_event_history_length)
                microphone->sound_event_history_index--;
        }
    }
}

static void thingz_microphone_timer_callback(void *args){
    thingz_microphone_obj_t *microphone = (thingz_microphone_obj_t *)args;

    uint8_t value = thingz_read_jack_mic();

    if(value < microphone->sound_event_quiet_level){
        thingz_microphone_history_push_event(microphone, THINGZ_MICROPHONE_EVENT_QUIET);
    }
    if(value > microphone->sound_event_loud_level){
        thingz_microphone_history_push_event(microphone, THINGZ_MICROPHONE_EVENT_LOUD);
    }
}

void thingz_microphone_init(thingz_microphone_obj_t* microphone, const mcu_pin_obj_t* pinJackMIC){
    claim_pin(pinJackMIC);

    microphone->pinJackMIC = pinJackMIC;

    microphone->base.type = &thingz_microphone_type;

    #if defined(DEBUG) || defined(ENABLE_JTAG)
        //JTAG share pins with led
        return;
    #endif

    gpio_config_t io_conf;

    io_conf.pin_bit_mask = 1ull << pinJackMIC->number;
    io_conf.mode = GPIO_MODE_INPUT;


    gpio_reset_pin(pinJackMIC->number);

    gpio_config(&io_conf);

    adc2_config_channel_atten(ADC2_CHANNEL_5,ADC_ATTEN_DB_11);

    microphone->sound_event_loud_level = 126;
    microphone->sound_event_quiet_level = 127;
    microphone->sound_event_history_length = 5;
    microphone->sound_event_history_index = 0;
    microphone->sound_event_history = malloc(sizeof(uint8_t)*microphone->sound_event_history_length);

    int i;
    for(i = 0; i < microphone->sound_event_history_length; i++){
        microphone->sound_event_history[i] = THINGZ_MICROPHONE_EVENT_NONE;
    }

    mic_timer_args.arg = (void*)microphone;
    esp_timer_create(&mic_timer_args, &mic_timer);
    esp_timer_start_periodic(mic_timer, 250);

}

void thingz_microphone_deinit(thingz_microphone_obj_t* microphone){
    #if defined(DEBUG) || defined(ENABLE_JTAG)
        //JTAG share pins with sound
        return;
    #endif

    gpio_reset_pin(microphone->pinJackMIC->number);

    free(microphone->sound_event_history);

    common_hal_reset_pin(microphone->pinJackMIC);

    if(mic_timer){
        esp_timer_stop(mic_timer);
        esp_timer_delete(mic_timer);
    }
}

//NEW
static mp_obj_t mp_thingz_microphone_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_microphone_obj_t *self = m_new_obj_with_finaliser(thingz_microphone_obj_t);
    
    self->base.type = &thingz_microphone_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_microphone_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_microphone_del_obj, mp_thingz_microphone_del);

//CURRENT EVENT
static mp_obj_t mp_thingz_microphone_current_event(mp_obj_t self_in) {
	thingz_microphone_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int8_t index = self->sound_event_history_index == self->sound_event_history_length - 1 ? self->sound_event_history_index : self->sound_event_history_index - 1;
    //mp_printf(MP_PYTHON_PRINTER, "%d\n", index);
    if(index >= 0 && self->sound_event_history[index] == THINGZ_MICROPHONE_EVENT_LOUD){
        return (mp_obj_t)&thingz_sound_event_loud_obj;
    }else if(index >= 0 && self->sound_event_history[index] == THINGZ_MICROPHONE_EVENT_QUIET){
        return (mp_obj_t)&thingz_sound_event_quiet_obj;
    }else{
        return (mp_obj_t)&thingz_sound_event_none_obj;
    }
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_microphone_current_event_obj, mp_thingz_microphone_current_event);

//WAS EVENT
static mp_obj_t mp_thingz_microphone_was_event(mp_obj_t self_in, mp_obj_t event) {
	thingz_microphone_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    thingz_sound_event_obj_t *sound_event = MP_OBJ_TO_PTR(event);
    bool result = false;
    int i;
    
    if(sound_event == &thingz_sound_event_loud_obj || sound_event == &thingz_sound_event_quiet_obj || sound_event == &thingz_sound_event_none_obj){
        uint8_t target;
        if(sound_event == &thingz_sound_event_loud_obj)
            target = THINGZ_MICROPHONE_EVENT_LOUD;
        else if(sound_event == &thingz_sound_event_quiet_obj)
            target = THINGZ_MICROPHONE_EVENT_QUIET;
        else
            target = THINGZ_MICROPHONE_EVENT_NONE;
        
        for(i = 0; i <= self->sound_event_history_index; i++){
            if(self->sound_event_history[i] == target){
                result = true;
                break;
            }
        }
    }

    for(i = 0; i < self->sound_event_history_length; i++){
        self->sound_event_history[i] = THINGZ_MICROPHONE_EVENT_NONE;
    }

    self->sound_event_history_index = 0;

    return mp_obj_new_bool(result);
}

MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_microphone_was_event_obj, mp_thingz_microphone_was_event);

//SOUND LEVEL
static mp_obj_t mp_thingz_microphone_sound_level(mp_obj_t self_in) {
	// thingz_microphone_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int val;
    
    val = thingz_read_jack_mic();

    return mp_obj_new_int(val);

}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_microphone_sound_level_obj, mp_thingz_microphone_sound_level);

//IS EVENT
static mp_obj_t mp_thingz_microphone_is_event(mp_obj_t self_in, mp_obj_t event) {    
    thingz_sound_event_obj_t *sound_event = MP_OBJ_TO_PTR(event);
    
    thingz_sound_event_obj_t *last_event = mp_thingz_microphone_current_event(self_in);
    
    return mp_obj_new_bool(last_event == sound_event);
}

MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_microphone_is_event_obj, mp_thingz_microphone_is_event);

//GET EVENTS
static mp_obj_t mp_thingz_microphone_get_events(mp_obj_t self_in) {
	thingz_microphone_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    mp_obj_t events[self->sound_event_history_index+1];

    int i = 0;
    
    while(i <= self->sound_event_history_index && self->sound_event_history[i] != THINGZ_MICROPHONE_EVENT_NONE){
        events[i] = self->sound_event_history[i] == THINGZ_MICROPHONE_EVENT_LOUD ? (mp_obj_t)&thingz_sound_event_loud_obj : (mp_obj_t)&thingz_sound_event_quiet_obj;
        i++;
    }

    int j = 0;
    for(j = 0; j < self->sound_event_history_length; j++){
        self->sound_event_history[j] = THINGZ_MICROPHONE_EVENT_NONE;
    }

    return mp_obj_new_list(i, events);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_microphone_get_events_obj, mp_thingz_microphone_get_events);

//SET THRESHOLD
static mp_obj_t mp_thingz_microphone_set_threshold(mp_obj_t self_in, mp_obj_t event, mp_obj_t value) {
	thingz_microphone_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    thingz_sound_event_obj_t *sound_event = MP_OBJ_TO_PTR(event);

    int v = mp_obj_get_int(value);

    if(v < 0)
        v = 0;
    else if(v > 255){
        v = 255;
    }

    if(sound_event == &thingz_sound_event_loud_obj){
        self->sound_event_loud_level = v;
    }else if(sound_event == &thingz_sound_event_quiet_obj){
        self->sound_event_quiet_level = v;
    }
    
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_3(mp_thingz_microphone_set_threshold_obj, mp_thingz_microphone_set_threshold);

static const mp_map_elem_t thingz_microphone_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__),             (mp_obj_t)&mp_thingz_microphone_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_current_event),       (mp_obj_t)&mp_thingz_microphone_current_event_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_was_event),           (mp_obj_t)&mp_thingz_microphone_was_event_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sound_level),         (mp_obj_t)&mp_thingz_microphone_sound_level_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_is_event),            (mp_obj_t)&mp_thingz_microphone_is_event_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_events),          (mp_obj_t)&mp_thingz_microphone_get_events_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_threshold),       (mp_obj_t)&mp_thingz_microphone_set_threshold_obj }
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_microphone_local_dict,
	thingz_microphone_local_dict_table
);

const mp_obj_type_t thingz_microphone_type = {
.base = { &mp_type_type },
.name = MP_QSTR_Microphone,
.make_new = mp_thingz_microphone_make_new,
.locals_dict = (mp_obj_t)&mp_thingz_microphone_local_dict,
};


const mp_obj_type_t thingz_sound_event_type;

const thingz_sound_event_obj_t thingz_sound_event_loud_obj = {
    { &thingz_sound_event_type },
};

const thingz_sound_event_obj_t thingz_sound_event_quiet_obj = {
    { &thingz_sound_event_type },
};

const thingz_sound_event_obj_t thingz_sound_event_none_obj = {
    { &thingz_sound_event_type },
};

static const mp_rom_map_elem_t thingz_sound_event_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_QUIET),       MP_ROM_PTR(&thingz_sound_event_quiet_obj) },
    { MP_ROM_QSTR(MP_QSTR_LOUD),        MP_ROM_PTR(&thingz_sound_event_loud_obj) },
    { MP_ROM_QSTR(MP_QSTR_NONE),        MP_ROM_PTR(&thingz_sound_event_none_obj) },
};
static MP_DEFINE_CONST_DICT(thingz_sound_event_locals_dict, thingz_sound_event_locals_dict_table);

static void thingz_sound_event_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    qstr sound_event = MP_QSTR_QUIET;
    if (MP_OBJ_TO_PTR(self_in) == MP_ROM_PTR(&thingz_sound_event_loud_obj)) {
        sound_event = MP_QSTR_LOUD;
    }
    mp_printf(print, "%q.%q.%q", MP_QSTR_thingz, MP_QSTR_SoundEvent, sound_event);
}

const mp_obj_type_t thingz_sound_event_type = {
    { &mp_type_type },
    .name = MP_QSTR_SoundEvent,
    .print = thingz_sound_event_print,
    .locals_dict = (mp_obj_t)&thingz_sound_event_locals_dict,
};


MP_DEFINE_CONST_OBJ_TYPE(
    thingz_sound_event_type,
    MP_QSTR_SoundEvent,
    MP_TYPE_FLAG_NONE,
    print, thingz_sound_event_print
    locals_dict, &thingz_sound_event_locals_dict
);
