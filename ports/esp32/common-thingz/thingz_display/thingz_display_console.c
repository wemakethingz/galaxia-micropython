#include "thingz_display_console.h"

#include "common-thingz/thingz_screen/thingz_screen.h"


#include "py/runtime.h"
#include "py/objstr.h"


void thingz_display_console_init(thingz_display_console_obj_t* console){
    console->base.type = &thingz_display_console_type;
}

void thingz_display_console_deinit(thingz_display_console_obj_t* console){
    
}

//NEW
static mp_obj_t mp_thingz_display_console_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_display_console_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_display_console_obj_t));
    
    self->base.type = &thingz_display_console_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_display_console_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_console_del_obj, mp_thingz_display_console_del);


//SHOW
static mp_obj_t mp_thingz_display_console_show(mp_obj_t self_in){
    thingz_screen_switch_mode(COMMON_THINGZ_SCREEN_MODE_REPL);
    
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_console_show_obj, mp_thingz_display_console_show);


static const mp_map_elem_t thingz_display_console_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__),     (mp_obj_t)&mp_thingz_display_console_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_show),        (mp_obj_t)&mp_thingz_display_console_show_obj },  
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_display_console_local_dict,
	thingz_display_console_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_display_console_type,
    MP_QSTR_Console,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_display_console_make_new,
    locals_dict, &mp_thingz_display_console_local_dict
);