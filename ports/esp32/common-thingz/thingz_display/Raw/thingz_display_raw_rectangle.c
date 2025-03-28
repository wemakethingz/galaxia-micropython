#include <stdio.h>
#include <string.h>

#include "common-thingz/thingz_display/Raw/thingz_display_raw_rectangle.h"
#include "common-thingz/thingz_screen/thingz_screen.h"
#include "common-thingz/thingz_screen/thingz_screen_raw.h"

#include "py/obj.h"
#include "py/runtime.h"

//|
//| """ Thingz Display Raw Rect
//| """
//|
//| class Rect:
//|    """
//|    Create an rectangle and print it to the screen
//|    """
//|
//|    def __init__(self, x:int, y:int, width:int, height:int, color:int) -> None:
//|        """
//|        Create a rectangle and print it to the screen
//|        :param int x: X position
//|        :param int y: Y position
//|        :param int width: width
//|        :param int height: height
//|        :param int color: Color
//|        """
//|        ...
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
//|    def show(self, show:bool) -> None:
//|        """
//|        Show/Hide the rectangle
//|        
//|        :param bool show: Show the rectangle if True hide if False
//|        """
//|        ...
//|
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

//|    def x(self, pos:int) -> None:
//|        """
//|        Set x position
//|        
//|        :param int pos: x position
//|        """
//|        ...
//|
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

//|    def y(self, pos:int) -> None:
//|        """
//|        Set y position
//|        
//|        :param int pos: y position
//|        """
//|        ...
//|
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


//|    def color(self, color:int) -> None:
//|        """
//|        Set rectangle color
//|        
//|        :param int color: color
//|        """
//|        ...
//|
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

//|    def get_show(self) -> bool:
//|        """
//|        Return True if the rectangle is shown
//|        
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_rectangle_get_show(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_bool(self->show);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_show_obj, mp_thingz_display_raw_rectangle_get_show);


//|    def get_x(self) -> int:
//|        """
//|        Get x position
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_rectangle_get_x(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->x);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_x_obj, mp_thingz_display_raw_rectangle_get_x);

//|    def get_y(self) -> int:
//|        """
//|        Get y position
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_rectangle_get_y(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->y);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_y_obj, mp_thingz_display_raw_rectangle_get_y);

//|    def get_width(self) -> int:
//|        """
//|        Get image width
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_rectangle_get_width(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->width);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_width_obj, mp_thingz_display_raw_rectangle_get_width);

//|    def get_height(self) -> int:
//|        """
//|        Get image height
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_rectangle_get_height(mp_obj_t self_in) {
    thingz_display_raw_rectangle_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->height);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_rectangle_get_height_obj, mp_thingz_display_raw_rectangle_get_height);

//|    def get_color(self) -> int:
//|        """
//|        Get color
//|        """
//|        ...
//|
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