#include "thingz_display.h"
#include "thingz_display_mode.h"
#include "common-thingz/thingz_display/Plot/thingz_display_plot.h"
#include "common-thingz/thingz_display/Console/thingz_display_console.h"
#include "common-thingz/thingz_display/Raw/thingz_display_raw.h"

#include "common-thingz/thingz_screen/thingz_screen.h"

#include "py/runtime.h"
#include "py/objstr.h"

thingz_display_plot_obj_t thingz_display_plot;
thingz_display_console_obj_t thingz_display_console;
thingz_display_raw_obj_t thingz_display_raw;


void thingz_display_init(thingz_display_obj_t* display){
    display->base.type = &thingz_display_type;
    thingz_display_plot_init(&thingz_display_plot);
    thingz_display_console_init(&thingz_display_console);
    thingz_display_raw_init(&thingz_display_raw);

}

void thingz_display_deinit(thingz_display_obj_t* display){
    thingz_display_plot_deinit(&thingz_display_plot);
    thingz_display_console_deinit(&thingz_display_console);
    thingz_display_raw_deinit(&thingz_display_raw);
}
//|
//| """ Thingz Display
//| """
//| 
//| class Display:
//|    """Control Galaxia LCD display"""
//| 
//|    plot: Plot
//|    """
//|    Plot data on the LCD
//|    This object is an instance of `Plot`
//|    """
//| 
//|    console: Console
//|    """
//|    Show REPL output on the LCD
//|    This object is an instance of `Console`
//|    """
//|    
//|    raw: Raw
//|    """
//|    Display graphical elements
//|    This object is an instance of `Raw`
//|    """
//| 
//NEW
static mp_obj_t mp_thingz_display_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_display_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_display_obj_t));
    
    self->base.type = &thingz_display_type;
    thingz_display_plot_init(&thingz_display_plot);
    thingz_display_console_init(&thingz_display_console);
    thingz_display_raw_init(&thingz_display_raw);
    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_display_del(mp_obj_t self_in) {
	thingz_display_plot_deinit(&thingz_display_plot);
    thingz_display_console_deinit(&thingz_display_console);
    thingz_display_raw_deinit(&thingz_display_raw);
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_del_obj, mp_thingz_display_del);


static const mp_map_elem_t thingz_display_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__),     (mp_obj_t)&mp_thingz_display_del_obj },  
    { MP_ROM_QSTR(MP_QSTR_plot),			MP_ROM_PTR(&thingz_display_plot)},  
    { MP_ROM_QSTR(MP_QSTR_console),			MP_ROM_PTR(&thingz_display_console)},  
    { MP_ROM_QSTR(MP_QSTR_raw),			MP_ROM_PTR(&thingz_display_raw)}, 

};

static MP_DEFINE_CONST_DICT (
	mp_thingz_display_local_dict,
	thingz_display_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_display_type,
    MP_QSTR_Display,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_display_make_new,
    locals_dict, &mp_thingz_display_local_dict
);