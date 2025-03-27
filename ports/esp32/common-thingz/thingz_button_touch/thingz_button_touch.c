#include "thingz_button_touch.h"

#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objfun.h"
#include "py/bc.h"

#include "driver/touch_pad.h"

#include "esp_timer.h"
#include "esp_log.h"

#include "FreeRTOS.h"
#include "task.h"

#include "common-thingz/thingz/thingz.h"

#define THINGZ_BUTTON_TOUCH_REGISTERED_LENGTH 10
#define THINGZ_BUTTON_TOUCH_SENSITIVITY 0.62

static const char* TAG = "THGZ_BUTTON_TOUCH";

static thingz_button_touch_obj_t* thingz_registered_touch_buttons[THINGZ_BUTTON_TOUCH_REGISTERED_LENGTH];

static bool init = false;
// static esp_timer_create_args_t touch_debounce_timer_args;
// static esp_timer_handle_t touch_debounce_timer;
// static uint32_t touch_debouce_timer_args_args[2];

static void _thingz_button_touch_calibrate(thingz_button_touch_obj_t* button){
    uint32_t touch_value = 0xFFFFFFFF;
    esp_err_t err;
    err = touch_pad_read_benchmark(button->pin, &touch_value);
    if(button->first_thresh == 0){
        button->first_thresh = touch_value;
    }
    if(err != ESP_OK){
        // mp_printf(MP_PYTHON_PRINTER, "ERROR: %d\n", err);
    }
    if(touch_value != 0x3FFFFF){
        if(button->thresh == 0){
            button->thresh = touch_value;
        }else if(!button->state){
            button->thresh = (button->thresh + touch_value) /2;
        }
    }
    else{
        // FSM failed reinit it
        // mp_printf(MP_PYTHON_PRINTER, "REINIT\n");
        int i;
        for(i = 0; i < 10; i++){
            touch_pad_fsm_stop();
            touch_pad_fsm_start();
            vTaskDelay(pdMS_TO_TICKS(50));
            touch_pad_filter_read_smooth(button->pin, &touch_value);
            if(touch_value != 0x3FFFFF){
                button->thresh = touch_value;
                break;
            }else{
                // mp_printf(MP_PYTHON_PRINTER, "REINIT FAILED\n");
            }
        }
    }
    // ESP_LOGI(TAG, "NUMBER: %d THRESH: %d\n", button->pin, button->thresh);
    // mp_printf(MP_PYTHON_PRINTER, "INIT %d INIT BUTTON %d NUMBER: %d THRESH: %d FIRST: %d\n", init, button->init, button->pin->number, button->thresh, button->first_thresh);
    if(touch_pad_set_thresh(button->pin, button->thresh * THINGZ_BUTTON_TOUCH_SENSITIVITY) != ESP_OK){
        // mp_printf(MP_PYTHON_PRINTER, "FAILED THR\n");
    }
}

static void _thingz_button_touch_calibrate_callback(void* args){
    int i;
    for(i = 0; i < THINGZ_BUTTON_TOUCH_REGISTERED_LENGTH; i++){
        if(thingz_registered_touch_buttons[i] == 0)
            continue;
        _thingz_button_touch_calibrate(thingz_registered_touch_buttons[i]);
    }
}

static const esp_timer_create_args_t calibrate_timer_args = {
            .callback = &_thingz_button_touch_calibrate_callback,
            .name = "button touch calibrate"
};

static esp_timer_handle_t calibrate_timer;

//called by supervisor
static void thingz_button_touch_background_callback(void *args){
    thingz_button_touch_obj_t *button_touch = (thingz_button_touch_obj_t*)args;
    
    if(button_touch->touched_callback){
        mp_obj_t f_args[1];
        f_args[0] = button_touch;

        // mp_type_fun_bc.ext->call(button_touch->touched_callback, button_touch->touched_callback_num_args, 0, f_args);
        mp_sched_schedule(button_touch->touched_callback, button_touch);
    }
}
static void thingz_button_touch_debounce_callback(void *arg){
    // int i;
    // uint32_t intr_mask = *((uint32_t*)arg);
    uint32_t pad_status = touch_pad_get_status();
    // uint32_t pad_num = *((uint32_t*)arg+);
    thingz_button_touch_obj_t* button = (thingz_button_touch_obj_t*) arg;

    void (*cb)(thingz_input_event_t) = NULL;
    if(thingz_get_input_target() == THINGZ_INPUT_TARGET_TGZ_DEBUG){
        cb = thingz_get_input_event_callback();
    }

    if((pad_status >> button->pin) & 1){
        if(cb){
            thingz_input_event_t event;
            event.event = THINGZ_INPUT_EVENT_BUTTON_PRESS;
            event.obj = button;
            cb(event);
            return;
        }
        button->state = true;
    }else{
        if(cb){
            thingz_input_event_t event;
            event.event = THINGZ_INPUT_EVENT_BUTTON_RELEASE;
            event.obj = button;
            cb(event);
            return;
        }
        if(button->state){
            button->was_touched = true;
            button->touches_count++;

            if(button->touched_callback){
                thingz_button_touch_background_callback(button);
            }

        }
        button->state = false;
    }

    // for(i = 0; i < THINGZ_BUTTON_TOUCH_REGISTERED_LENGTH; i++){
    //     if(thingz_registered_touch_buttons[i] == 0)
    //         continue;
    //     // mp_printf(MP_PYTHON_PRINTER, "BUTTON %d mask %d, pad %d\n", thingz_registered_touch_buttons[i]->pin->number, intr_mask, pad_num);
    //     // if(esp_timer_get_time()-thingz_registered_touch_buttons[i]->timestamp < 90){
    //     //     continue;
    //     // }
    //     // thingz_registered_touch_buttons[i]->timestamp = esp_timer_get_time();
    //     if(pad_num == (uint32_t)thingz_registered_touch_buttons[i]->pin->number){
    //         if(intr_mask & TOUCH_PAD_INTR_MASK_ACTIVE){
    //             if(cb){
    //                 thingz_input_event_t event;
    //                 event.event = THINGZ_INPUT_EVENT_BUTTON_PRESS;
    //                 event.obj = thingz_registered_touch_buttons[i];
    //                 cb(event);
    //                 continue;
    //             }
    //             thingz_registered_touch_buttons[i]->state = true;
    //         }else{
                

    //         }
    //     }
    // }
}

static void thingz_button_touch_interupt_cb(void *arg)
{
    uint32_t pad_num = touch_pad_get_current_meas_channel();
    int i;
    for(i = 0; i < THINGZ_BUTTON_TOUCH_REGISTERED_LENGTH; i++){
        if( thingz_registered_touch_buttons[i] && pad_num == (uint32_t)thingz_registered_touch_buttons[i]->pin){
            esp_timer_start_once(thingz_registered_touch_buttons[i]->touch_debounce_timer, 1000);
        }
    }
}

void thingz_button_touch_common_init(void){

    int i;
    for(i = 0; i < THINGZ_BUTTON_TOUCH_REGISTERED_LENGTH; i++){
        thingz_registered_touch_buttons[i] = 0;
    }
    init = true;

    if(touch_pad_init() != ESP_OK){
        init = false;
    }

    /* Denoise setting at TouchSensor 0. */
    touch_pad_denoise_t denoise = {
        /* The bits to be cancelled are determined according to the noise level. */
        .grade = TOUCH_PAD_DENOISE_BIT4,
        /* By adjusting the parameters, the reading of T0 should be approximated to the reading of the measured channel. */
        .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
    };
    
    if(touch_pad_denoise_set_config(&denoise) != ESP_OK){
        init = false;
    }

    if(touch_pad_denoise_enable() != ESP_OK){
        init = false;
    }

    /* Filter setting */

    touch_filter_config_t filter_info = {
        .mode = TOUCH_PAD_FILTER_IIR_16,           // Test jitter and filter 1/4.
        .debounce_cnt = 1,      // 1 time count.
        // .hysteresis_thr = 3,    // 3%
        .noise_thr = 0,         // 50%
        // .noise_neg_thr = 0,     // 50%
        // .neg_noise_limit = 10,  // 10 time count.
        .jitter_step = 4,       // use for jitter mode.
        .smh_lvl = TOUCH_PAD_SMOOTH_IIR_2,
    };
    if(touch_pad_filter_set_config(&filter_info) != ESP_OK){
        init = false;
    }
    
    if(touch_pad_filter_enable() != ESP_OK){
        init = false;
    }

    if(touch_pad_timeout_set(true, SOC_TOUCH_PAD_THRESHOLD_MAX) != ESP_OK){
        init = false;
    }
    
    if(touch_pad_isr_register(thingz_button_touch_interupt_cb, NULL, TOUCH_PAD_INTR_MASK_ALL) != ESP_OK){
        init = false;
    }

    if(touch_pad_intr_enable(TOUCH_PAD_INTR_MASK_ACTIVE | TOUCH_PAD_INTR_MASK_INACTIVE | TOUCH_PAD_INTR_MASK_TIMEOUT) != ESP_OK){
        init = false;
    }
    
    /* Enable touch sensor clock. Work mode is "timer trigger". */
    if(touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER) != ESP_OK){
        init = false;
    }

    if(touch_pad_fsm_start() != ESP_OK){
        init = false;
    }
    
    esp_timer_create(&calibrate_timer_args, &calibrate_timer);
    esp_timer_start_periodic(calibrate_timer, 2000000);

}

void thingz_button_touch_common_deinit(void){
    init = false;
    if(touch_pad_fsm_stop() != ESP_OK){
        mp_printf(MP_PYTHON_PRINTER, "DEINIT FAILED\n");   
    }
    touch_pad_reset();
    esp_timer_stop(calibrate_timer);
    esp_timer_delete(calibrate_timer);

    int i;
    for(i = 0; i < THINGZ_BUTTON_TOUCH_REGISTERED_LENGTH; i++){
        if(thingz_registered_touch_buttons[i]){
            esp_timer_stop(thingz_registered_touch_buttons[i]->touch_debounce_timer);
            esp_timer_delete(thingz_registered_touch_buttons[i]->touch_debounce_timer);
        }
        thingz_registered_touch_buttons[i] = 0;
        
    }
}

void thingz_button_touch_init(thingz_button_touch_obj_t* button, uint8_t pin){
    button->init = true;
    button->state = false;
    button->pin = pin;
    button->base.type = &thingz_button_touch_type;
    button->thresh = 0;
    button->touched_callback = NULL;

    button->touch_debounce_timer_args.callback = &thingz_button_touch_debounce_callback;
    button->touch_debounce_timer_args.name = "touch button debounce";
    button->touch_debounce_timer_args.arg = button;
    
    esp_timer_create(&button->touch_debounce_timer_args, &button->touch_debounce_timer);

    if(touch_pad_config(button->pin) != ESP_OK){
        button->init = false;
    }

    int i = 0;
    for(i = 0; i < THINGZ_BUTTON_TOUCH_REGISTERED_LENGTH; i++){
        if(thingz_registered_touch_buttons[i] == 0){
            thingz_registered_touch_buttons[i] = button;
            break;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    _thingz_button_touch_calibrate(button);
}

void thingz_button_touch_deinit(thingz_button_touch_obj_t* button_touch){
    int i = 0;

    esp_timer_stop(button_touch->touch_debounce_timer);
    esp_timer_delete(button_touch->touch_debounce_timer);
    for(i = 0; i < THINGZ_BUTTON_TOUCH_REGISTERED_LENGTH; i++){
        if(thingz_registered_touch_buttons[i] == button_touch){
            thingz_registered_touch_buttons[i] = 0;
            break;
        }
    }
}

//| 
//| 
//| """ Thingz button touch
//| """
//| 
//| class ButtonTouch:
//|    """Control Galaxia's touch buttons"""
//| 
//| 

//NEW
static mp_obj_t mp_thingz_button_touch_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_button_touch_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_button_touch_obj_t));
    self->state = false;
    self->base.type = &thingz_button_touch_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_button_touch_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_button_touch_del_obj, mp_thingz_button_touch_del);

//IS TOUCHED
//|    def is_touched(self) -> bool:
//|        """
//|        :return: True if button is touched, False otherwise
//|        :rtype: bool
//|        """
//|        ...
//| 
static mp_obj_t mp_thingz_button_touch_is_touched(mp_obj_t self_in) {
    thingz_button_touch_obj_t *self = MP_OBJ_TO_PTR(self_in);

    // uint32_t touch_value;
    // touch_pad_filter_read_smooth(self->pin->number, &touch_value);
    // mp_printf(MP_PYTHON_PRINTER, "Touch value: %d\n", touch_value);
	return mp_obj_new_bool(self->state);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_button_touch_is_touched_obj, mp_thingz_button_touch_is_touched);

//WAS TOUCHED
//|    def was_touched(self) -> bool:
//|        """
//|        :return: True if button has been touched since last call, False otherwise
//|        :rtype: bool
//|        """
//|        ...
//| 
static mp_obj_t mp_thingz_button_touch_was_touched(mp_obj_t self_in) {
	thingz_button_touch_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bool touched = self->was_touched;
    self->was_touched = false;
	return mp_obj_new_bool(touched);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_button_touch_was_touched_obj, mp_thingz_button_touch_was_touched);

//GET TOUCHES
//|    def get_touches(self) -> int:
//|        """
//|        Get the number of touches since the last call
//| 
//|        :return: The number of touches since the last call
//|        :rtype: int
//|        """
//|        ...
//| 
static mp_obj_t mp_thingz_button_touch_get_touches(mp_obj_t self_in) {
	thingz_button_touch_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint32_t touches = self->touches_count;
    self->touches_count = 0;
	return mp_obj_new_int(touches);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_button_touch_get_touches_obj, mp_thingz_button_touch_get_touches);

//ON TOUCHED
//|    def on_touched(self, callback: Callable[Optional[Button]]) -> None:
//|        """Register a callaback bind to touch event
//| 
//|        :param Callable[Optional[ButtonTouch]] callback: The function to call when the event occurs. When called the button will be passed as paramater""" 
//|        ...
//| 
static mp_obj_t mp_thingz_button_touch_on_touched(mp_obj_t self_in, mp_obj_t function) {

    thingz_button_touch_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    if(!mp_obj_is_type(function, &mp_type_fun_bc)) {
        mp_raise_TypeError("argument 'function': wrong prototype");
    }

    mp_obj_fun_bc_t *fun = MP_OBJ_TO_PTR(function);
    const byte *bc = fun->bytecode;
    MP_BC_PRELUDE_SIG_DECODE(bc);

    if(n_pos_args > 1){
        mp_raise_TypeError("argument 'function': fuction has too many args, max 1");
    }

    self->touched_callback_num_args = n_pos_args;
    self->touched_callback = fun;

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_button_touch_on_touched_obj, mp_thingz_button_touch_on_touched);


static const mp_map_elem_t thingz_button_touch_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR_is_touched), (mp_obj_t)&mp_thingz_button_touch_is_touched_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_was_touched), (mp_obj_t)&mp_thingz_button_touch_was_touched_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_touches), (mp_obj_t)&mp_thingz_button_touch_get_touches_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_on_touch_touched), (mp_obj_t)&mp_thingz_button_touch_on_touched_obj },    
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__), (mp_obj_t)&mp_thingz_button_touch_del_obj },
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_button_touch_local_dict,
	thingz_button_touch_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_button_touch_type,
    MP_QSTR_ButtonTouch,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_button_touch_make_new,
    locals_dict, &mp_thingz_button_touch_local_dict
);