#include "thingz_display_plot.h"

#include "common-thingz/thingz_screen/thingz_screen.h"

#include "py/objint.h"
#include "py/mpz.h"
#include "py/runtime.h"
#include "py/objstr.h"


void thingz_display_plot_init(thingz_display_plot_obj_t* plot){
//     int i;
    plot->base.type = &thingz_display_plot_type;
//     plot->next_point_x = 0;
//     plot->scale[0] = 0;
//     plot->scale[1] = 100;
}

void thingz_display_plot_deinit(thingz_display_plot_obj_t* plot){
    thingz_screen_plot_set_animate_function(&(thingz_screen.plot), NULL, 0);
}

//|
//| """ Thingz Display Plot
//| """
//|
//| class Plot:
//|    """
//|    Use the LCD as a plot
//|    """
//|

//NEW
static mp_obj_t mp_thingz_display_plot_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_display_plot_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_display_plot_obj_t));
    
    self->base.type = &thingz_display_plot_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_display_plot_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_plot_del_obj, mp_thingz_display_plot_del);


//SHOW
//|    def show(self) -> None:
//|        """
//|        Show the plot
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_plot_show(mp_obj_t self_in){
    thingz_screen_switch_mode(COMMON_THINGZ_SCREEN_MODE_PLOT);
    
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_plot_show_obj, mp_thingz_display_plot_show);

//ADD POINT
//|    def add_point(self, value: int|float) -> None:
//|        """
//|        Add a new point to the plot.
//|
//|        :param int|float value: The position on the Y axis of new point
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_plot_add_point(mp_obj_t self_in, mp_obj_t value) {

    float arg=0;
    const mp_obj_type_t* type = mp_obj_get_type(value);
    thingz_display_plot_obj_t* plot = (thingz_display_plot_obj_t*)self_in;
    if(mp_obj_is_int(value)){
        arg =  mp_obj_get_int(value);
    }else if(type == &mp_type_float){
        arg = mp_obj_get_float(value);
    }else if(type == &mp_type_int){
        mp_uint_t mpint;
        mpz_t mp = ((mp_obj_int_t*)value)->mpz;
        if(mpz_as_uint_checked(&mp, &mpint)){
            arg = (float)mpint;
        }else{
            mp_raise_TypeError(("argument 'value': cannot get long"));
        }
    }
    thingz_screen_plot_add_point(&(thingz_screen.plot), arg);
    
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_display_plot_add_point_obj, mp_thingz_display_plot_add_point);

//SET Y SCALE
//|    def set_y_scale(self, min: int, max: int) -> None:
//|        """
//|        Set the scale of the plot
//|
//|        :param int min: The min value of the Y axis
//|        :param int max: The max value of the Y axis
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_plot_set_y_scale(mp_obj_t self_in, mp_obj_t min, mp_obj_t max) {

    thingz_display_plot_obj_t* plot = (thingz_display_plot_obj_t*)self_in;

    const mp_obj_type_t* type = mp_obj_get_type(min);
    int32_t scale[2];

    if(mp_obj_is_int(min)){
        scale[0] = mp_obj_get_int(min);
    }else if(type == &mp_type_float){
        scale[0] = (int32_t)mp_obj_get_float(min);
    }else if(type == &mp_type_int){
        mp_uint_t mpint;
        mpz_t mp = ((mp_obj_int_t*)min)->mpz;
        if(mpz_as_uint_checked(&mp, &mpint)){
            scale[0] = (int32_t)mpint;
        }else{
            mp_raise_TypeError(("argument 'min': cannot get long"));
        }
    }

    type = mp_obj_get_type(max);

    if(mp_obj_is_int(max)){
        scale[1] =  mp_obj_get_int(max);
    }else if(type == &mp_type_float){
        scale[1] = (int32_t)mp_obj_get_float(max);
    }else if(type == &mp_type_int){
        mp_uint_t mpint;
        mpz_t mp = ((mp_obj_int_t*)max)->mpz;
        if(mpz_as_uint_checked(&mp, &mpint)){
            scale[0] = (uint8_t)mpint;
        }else{
            mp_raise_TypeError(("argument 'max': cannot get long"));
        }
    }
    thingz_screen_plot_set_scale(&(thingz_screen.plot), scale);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(mp_thingz_display_plot_set_y_scale_obj, mp_thingz_display_plot_set_y_scale);

//SET ANIMATE FUNCTION
//|    def set_animate_function(self, func: Callable, interval: int) -> None:
//|        """
//|        Configure a function that will be called once every interval to add a point to the plot. The function must return the value of the new point
//|
//|        :param Callable func: The function to call
//|        :param int interval: Time to wait between each function call, in seconds
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_plot_set_animate_function(mp_obj_t self_in, mp_obj_t fun, mp_obj_t interval) {

    thingz_display_plot_obj_t* plot = (thingz_display_plot_obj_t*)self_in;
    
    if(!mp_obj_is_type(fun, &mp_type_fun_bc)) {
        mp_raise_TypeError(("argument 'animate': wrong prototype"));
    }
    uint32_t i = 0;
    if(mp_obj_is_float(interval)){
        float inter = mp_obj_get_float(interval);
        i = inter;
    }else{
        i = mp_obj_get_int(interval);
    }
     
    thingz_screen_plot_set_animate_function(&(thingz_screen.plot), fun, i);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(mp_thingz_display_plot_set_animate_function_obj, mp_thingz_display_plot_set_animate_function);


static const mp_map_elem_t thingz_display_plot_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__),                 (mp_obj_t)&mp_thingz_display_plot_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_show),                    (mp_obj_t)&mp_thingz_display_plot_show_obj },  
    { MP_OBJ_NEW_QSTR(MP_QSTR_add_point),               (mp_obj_t)&mp_thingz_display_plot_add_point_obj },  
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_y_scale),             (mp_obj_t)&mp_thingz_display_plot_set_y_scale_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_animate_function),    (mp_obj_t)&mp_thingz_display_plot_set_animate_function_obj },  

};

static MP_DEFINE_CONST_DICT (
	mp_thingz_display_plot_local_dict,
	thingz_display_plot_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_display_plot_type,
    MP_QSTR_Plot,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_display_plot_make_new,
    locals_dict, &mp_thingz_display_plot_local_dict
);