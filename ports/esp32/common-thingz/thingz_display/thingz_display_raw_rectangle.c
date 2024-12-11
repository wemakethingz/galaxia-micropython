#include <stdio.h>
#include <string.h>

#include "common-thingz/thingz_display/thingz_display_raw_rectangle.h"
#include "common-thingz/thingz_screen/thingz_screen.h"
#include "common-thingz/thingz_screen/thingz_screen_raw.h"

#include "py/obj.h"
#include "py/runtime.h"


mp_obj_t mp_thingz_display_raw_rectangle_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 5, 5, false);
    // get the wanted pin object
    if(n_args < 5){
        mp_raise_ValueError(MP_ERROR_TEXT("invalid args"));
        return mp_const_none;
    }
    int x = mp_obj_get_int(args[0]);
    int y = mp_obj_get_int(args[1]);
    int width = mp_obj_get_int(args[2]);
    int height = mp_obj_get_int(args[3]);
    uint32_t color = mp_obj_get_int(args[4]);

    thingz_display_raw_rectangle_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_display_raw_rectangle_obj_t));
    self->base.type = type;
    self->x = x;
    self->y = y;
    self->screen_x = x;
    self->screen_y = y;
    self->color = color;
    self->show = 0;
    self->screen_show = 0;
    self->screen_height = height;
    self->screen_width = width;
    self->height = self->screen_height;
    self->width = self->screen_width;
    thingz_screen_raw_add_show_obj(&(thingz_screen.raw), self);
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mp_thingz_display_raw_rectangle_show(mp_obj_t self_in, mp_obj_t show) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if(!mp_obj_is_bool(show)){
        mp_raise_ValueError(MP_ERROR_TEXT("invalid arg type"));
        return mp_const_none;
    }
    if(show == mp_const_false){
        self->show = 0;
    }else{
        self->show = 1;
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_display_raw_rectangle_show_obj, mp_thingz_display_raw_rectangle_show);


static mp_obj_t mp_thingz_display_raw_rectangle_x(mp_obj_t self_in, mp_obj_t x) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if(!mp_obj_is_int(x)){
        mp_raise_ValueError(MP_ERROR_TEXT("invalid arg type"));
        return mp_const_none;
    }
    uint8_t new_x = mp_obj_get_int(x);
    
    self->x = new_x;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_display_raw_rectangle_x_obj, mp_thingz_display_raw_rectangle_x);

static mp_obj_t mp_thingz_display_raw_rectangle_y(mp_obj_t self_in, mp_obj_t y) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if(!mp_obj_is_int(y)){
        mp_raise_ValueError(MP_ERROR_TEXT("invalid arg type"));
        return mp_const_none;
    }
    uint8_t new_y = mp_obj_get_int(y);
    self->y = new_y;
    
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_display_raw_rectangle_y_obj, mp_thingz_display_raw_rectangle_y);

static mp_obj_t mp_thingz_display_raw_rectangle_color(mp_obj_t self_in, mp_obj_t color) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if(!mp_obj_is_int(color)){
        mp_raise_ValueError(MP_ERROR_TEXT("invalid arg type"));
        return mp_const_none;
    }
    uint32_t new_color = mp_obj_get_int(color);
    self->color = new_color;
    
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_display_raw_rectangle_color_obj, mp_thingz_display_raw_rectangle_color);

static mp_obj_t mp_thingz_display_raw_rectangle_get_show(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_bool(self->show);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_show_obj, mp_thingz_display_raw_rectangle_get_show);

static mp_obj_t mp_thingz_display_raw_rectangle_get_x(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->x);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_x_obj, mp_thingz_display_raw_rectangle_get_x);

static mp_obj_t mp_thingz_display_raw_rectangle_get_y(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->y);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_y_obj, mp_thingz_display_raw_rectangle_get_y);

static mp_obj_t mp_thingz_display_raw_rectangle_get_width(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->width);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_width_obj, mp_thingz_display_raw_rectangle_get_width);

static mp_obj_t mp_thingz_display_raw_rectangle_get_height(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->height);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_height_obj, mp_thingz_display_raw_rectangle_get_height);

static mp_obj_t mp_thingz_display_raw_rectangle_get_color(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->color);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_color_obj, mp_thingz_display_raw_rectangle_get_color);

//DEL
static mp_obj_t mp_thingz_screen_raw_rectangle_del(mp_obj_t self_in) {
	thingz_screen_raw_remove_show_obj(&(thingz_screen.raw), self_in);
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_screen_raw_rectangle_del_obj, mp_thingz_screen_raw_rectangle_del);


static const mp_rom_map_elem_t mp_thingz_display_raw_rectangle_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_Rect) },
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__), (mp_obj_t)&mp_thingz_screen_raw_rectangle_del_obj },
    { MP_ROM_QSTR(MP_QSTR_show), (mp_obj_t)(&mp_thingz_display_raw_rectangle_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_x), (mp_obj_t)(&mp_thingz_display_raw_rectangle_x_obj) },
    { MP_ROM_QSTR(MP_QSTR_y), (mp_obj_t)(&mp_thingz_display_raw_rectangle_y_obj) },
    { MP_ROM_QSTR(MP_QSTR_color), (mp_obj_t)(&mp_thingz_display_raw_rectangle_color_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_x), (mp_obj_t)(&mp_thingz_display_raw_rectangle_get_x_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_y), (mp_obj_t)(&mp_thingz_display_raw_rectangle_get_y_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_width), (mp_obj_t)(&mp_thingz_display_raw_rectangle_get_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_height), (mp_obj_t)(&mp_thingz_display_raw_rectangle_get_height_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_color), (mp_obj_t)(&mp_thingz_display_raw_rectangle_get_color_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_show), (mp_obj_t)(&mp_thingz_display_raw_rectangle_get_show_obj) },
};

static MP_DEFINE_CONST_DICT(mp_thingz_display_raw_rectangle_locals_dict, mp_thingz_display_raw_rectangle_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_thingz_display_raw_rectangle_type,
    MP_QSTR_Rect,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_display_raw_rectangle_make_new,
    locals_dict, &mp_thingz_display_raw_rectangle_locals_dict
);