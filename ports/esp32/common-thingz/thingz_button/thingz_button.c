#include "thingz_button.h"

#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objfun.h"
#include "py/bc.h"

#include "driver/gpio.h"

#include "common-thingz/thingz/thingz.h"

//called by supervisor
static void thingz_button_background_callback(void *args){
    thingz_button_obj_t *button = (thingz_button_obj_t*)args;
    
    if(button->pressed_callback){
        mp_obj_t f_args[1];
        f_args[0] = button;
        
        mp_sched_schedule(button->pressed_callback, button);
    }
}

static void thingz_button_debounce_callback(void* arg){
	thingz_button_obj_t *button = (thingz_button_obj_t*)arg;
    bool previous_state = button->state;
    bool new_state = !gpio_get_level(button->pin);

    if(thingz_get_input_target() == THINGZ_INPUT_TARGET_TGZ_DEBUG){
        void (*cb)(thingz_input_event_t) = thingz_get_input_event_callback();
        if(cb){
            thingz_input_event_t event;
            event.event = new_state == true ? THINGZ_INPUT_EVENT_BUTTON_PRESS : THINGZ_INPUT_EVENT_BUTTON_RELEASE;
            event.obj = button;
            cb(event);
        }
        gpio_intr_enable(button->pin);
        return;
    }

    button->state = !gpio_get_level(button->pin);
    if(previous_state && !button->state){
        if(esp_timer_get_time() - button->lastInterrupt < 1000)
            return;
        button->was_pressed = true;
        button->presses_count++;
        if(button->pressed_callback)
            thingz_button_background_callback(button);
        //     background_callback_add(&button->cb, thingz_button_background_callback, arg);
    }
    gpio_intr_enable(button->pin);
}

static void IRAM_ATTR thingz_button_interupt_cb(void* arg){
	thingz_button_obj_t *button = (thingz_button_obj_t*)arg;
    gpio_intr_disable(button->pin);
    esp_timer_start_once(button->debounce_timer, 50000);
}

void thingz_button_init(thingz_button_obj_t* button, uint8_t pin){

    button->state = false;
    button->pin = pin;
    button->pressed_callback = NULL;
    button->base.type = &thingz_button_type;

    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = 1ull << pin;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;

#if defined(DEBUG) || defined(ENABLE_JTAG)
    if(pin == 43){
        return;
    }
#endif

    gpio_reset_pin(pin);
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(pin, thingz_button_interupt_cb, (void*) button);
    button->state = !gpio_get_level(button->pin);

    const esp_timer_create_args_t timer_args = {
            .callback = &thingz_button_debounce_callback,
            .name = "button debounce",
            .arg = button
    };
    button->debounce_timer_args = timer_args;
    esp_timer_create(&button->debounce_timer_args, &button->debounce_timer);
    
}
//|
//|
//| """ Thingz button
//| """
//|
//|class Button:
//|    """Control Galaxia's physical buttons"""
//|
//|
//NEW
static mp_obj_t mp_thingz_button_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_button_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_button_obj_t));
    self->state = false;
    self->base.type = &thingz_button_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_button_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_button_del_obj, mp_thingz_button_del);

//IS PRESSED
//|    def is_pressed(self) -> bool:
//|        """
//|        :return: True if button is pressed, False otherwise
//|        :rtype: bool
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_button_is_pressed(mp_obj_t self_in) {
	thingz_button_obj_t *self = MP_OBJ_TO_PTR(self_in);
	return mp_obj_new_bool(self->state);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_button_is_pressed_obj, mp_thingz_button_is_pressed);

//WAS PRESSED
//|    def was_pressed(self) -> bool:
//|        """
//|        :return: True if button has been pressed since last call, False otherwise
//|        :rtype: bool
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_button_was_pressed(mp_obj_t self_in) {
	thingz_button_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bool pressed = self->was_pressed;
    self->was_pressed = false;
    // self->presses_count = 0;
	return mp_obj_new_bool(pressed);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_button_was_pressed_obj, mp_thingz_button_was_pressed);

//GET PRESSES
//|    def get_presses(self) -> int:
//|        """
//|        Get the number of presses since the last call
//|
//|        :return: The number of presses since the last call
//|        :rtype: int
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_button_get_presses(mp_obj_t self_in) {
	thingz_button_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint32_t pressed = self->presses_count;
    // self->was_pressed = false;
    self->presses_count = 0;
	return mp_obj_new_int(pressed);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_button_get_presses_obj, mp_thingz_button_get_presses);


//ON PRESSED
//|    def on_pressed(self, callback: Callable[Optional[Button]]) -> None:
//|        """Register a callaback bind to press event
//|
//|        :param Callable[Optional[Button]] callback: The function to call when the event occurs. When called the button will be passed as paramater""" 
//|        ...
//|
static mp_obj_t mp_thingz_button_on_pressed(mp_obj_t self_in, mp_obj_t function) {

    //  thingz_button_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    // if(mp_obj_is_type(function, &mp_type_fun_builtin_0) || mp_obj_is_type(function, &mp_type_fun_builtin_1){
    //     self->pressed_callback = MP_OBJ_TO_PTR(function);
    // }else{
    //     mp_raise_TypeError("argument 'function': wrong prototype");
    // }

    // return mp_const_none;
    thingz_button_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    if(!mp_obj_is_type(function, &mp_type_fun_bc)) {
        mp_raise_TypeError("argument 'function': wrong prototype");
    }

    mp_obj_fun_bc_t *fun = MP_OBJ_TO_PTR(function);

    const byte *bc = fun->bytecode;
    MP_BC_PRELUDE_SIG_DECODE(bc);

    if(n_pos_args > 1){
        mp_raise_TypeError("argument 'function': fuction has too many args, max 1");
    }

    self->pressed_callback_args_num = n_pos_args;

    self->pressed_callback = fun;

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_button_on_pressed_obj, mp_thingz_button_on_pressed);

static const mp_rom_map_elem_t thingz_button_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR_is_pressed), (mp_obj_t)&mp_thingz_button_is_pressed_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_was_pressed), (mp_obj_t)&mp_thingz_button_was_pressed_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_presses), (mp_obj_t)&mp_thingz_button_get_presses_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_on_button_pressed), (mp_obj_t)&mp_thingz_button_on_pressed_obj },    
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__), (mp_obj_t)&mp_thingz_button_del_obj },
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_button_local_dict,
	thingz_button_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_button_type,
    MP_QSTR_Button,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_button_make_new,
    locals_dict, &mp_thingz_button_local_dict
);