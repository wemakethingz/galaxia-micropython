#include <stdio.h>
#include <string.h>

#include "common-thingz/thingz_display/Raw/thingz_display_raw_text.h"
#include "common-thingz/thingz_screen/thingz_screen.h"
#include "common-thingz/thingz_screen/thingz_screen_raw.h"

#include "py/obj.h"
#include "py/runtime.h"

//|
//| """ Thingz Display Raw Text
//| """
//|
//| class Text:
//|
//|    def __init__(self, x:int, y:int, text:str, color:int) -> None:
//|        """
//|        Create an image and print it to the screen
//|
//|        :param int x: X position
//|        :param int y: Y position
//|        :param str text: text
//|        :param int color: color
//|        """
//|  
mp_obj_t mp_thingz_display_raw_text_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 4, 4, false);

    int x = mp_obj_get_int(args[0]);
    int y = mp_obj_get_int(args[1]);
    const char* t = mp_obj_str_get_str(args[2]);
    uint32_t color = mp_obj_get_int(args[3]);

    thingz_display_raw_text_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_display_raw_text_obj_t));
    self->base.type = type;
    self->x = x;
    self->y = y;
    self->screen_x = x;
    self->screen_y = y;
    self->color = color;
    self->show = 0;
    self->screen_show = 0;
    self->screen_height = thingz_screen.params.font_height;
    self->screen_width = thingz_screen.params.font_height*strlen(t);
    self->has_changed = 0;
    self->text = t;
    thingz_screen_raw_add_show_obj(&(thingz_screen.raw), self);
    return MP_OBJ_FROM_PTR(self);
}

//|    def show(self, show:bool) -> None:
//|        """
//|        Show/Hide the text
//|        
//|        :param bool show: Show the text if True hide if False
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_show(mp_obj_t self_in, mp_obj_t show) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if(!mp_obj_is_bool(show)){
        mp_raise_ValueError(MP_ERROR_TEXT("invalid arg type"));
    }
    self->show = (show == mp_const_true) ? 1 : 0;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_display_raw_text_show_obj, mp_thingz_display_raw_text_show);

//|    def x(self, pos:int) -> None:
//|        """
//|        Set x position
//|        
//|        :param int pos: x position
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_x(mp_obj_t self_in, mp_obj_t x) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    self->x = mp_obj_get_int(x);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_display_raw_text_x_obj, mp_thingz_display_raw_text_x);

//|    def y(self, pos:int) -> None:
//|        """
//|        Set y position
//|        
//|        :param int pos: y position
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_y(mp_obj_t self_in, mp_obj_t y) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    self->y = mp_obj_get_int(y);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_display_raw_text_y_obj, mp_thingz_display_raw_text_y);

//|    def set_text(self, text:str) -> None:
//|        """
//|        Set text
//|        
//|        :param str text: text
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_set_text(mp_obj_t self_in, mp_obj_t text) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    self->text = mp_obj_str_get_str(text);
    self->screen_width = thingz_screen.params.font_height*strlen(self->text);
    self->has_changed = 1;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_display_raw_text_set_text_obj, mp_thingz_display_raw_text_set_text);

//|    def get_show(self) -> bool:
//|        """
//|        Return True is text is shown else False
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_get_show(mp_obj_t self_in) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_bool(self->show);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_text_get_show_obj, mp_thingz_display_raw_text_get_show);

//|    def get_x(self) -> int:
//|        """
//|        Get x position
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_get_x(mp_obj_t self_in) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->x);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_text_get_x_obj, mp_thingz_display_raw_text_get_x);

//|    def get_y(self) -> int:
//|        """
//|        Get y position
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_get_y(mp_obj_t self_in) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->y);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_text_get_y_obj, mp_thingz_display_raw_text_get_y);

//|    def get_width(self) -> int:
//|        """
//|        Get text width
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_get_width(mp_obj_t self_in) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->screen_width);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_text_get_width_obj, mp_thingz_display_raw_text_get_width);

//|    def get_height(self) -> int:
//|        """
//|        Get text height
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_get_height(mp_obj_t self_in) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->screen_height);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_text_get_height_obj, mp_thingz_display_raw_text_get_height);

//|    def get_color(self) -> int:
//|        """
//|        Get text color
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_get_color(mp_obj_t self_in) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_int(self->color);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_text_get_color_obj, mp_thingz_display_raw_text_get_color);

//|    def get_text(self) -> int:
//|        """
//|        Get text
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_text_get_text(mp_obj_t self_in) {
    thingz_display_raw_text_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    return mp_obj_new_str(self->text, strlen(self->text));
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_text_get_text_obj, mp_thingz_display_raw_text_get_text);

//DEL
static mp_obj_t mp_thingz_screen_raw_text_del(mp_obj_t self_in) {
	thingz_screen_raw_remove_show_obj(&(thingz_screen.raw), self_in);
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_screen_raw_text_del_obj, mp_thingz_screen_raw_text_del);


static const mp_rom_map_elem_t mp_thingz_display_raw_text_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_Text) },
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__), (mp_obj_t)&mp_thingz_screen_raw_text_del_obj },
    { MP_ROM_QSTR(MP_QSTR_show), (mp_obj_t)(&mp_thingz_display_raw_text_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_x), (mp_obj_t)(&mp_thingz_display_raw_text_x_obj) },
    { MP_ROM_QSTR(MP_QSTR_y), (mp_obj_t)(&mp_thingz_display_raw_text_y_obj) },
    { MP_ROM_QSTR(MP_QSTR_text), (mp_obj_t)(&mp_thingz_display_raw_text_set_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_x), (mp_obj_t)(&mp_thingz_display_raw_text_get_x_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_y), (mp_obj_t)(&mp_thingz_display_raw_text_get_y_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_width), (mp_obj_t)(&mp_thingz_display_raw_text_get_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_height), (mp_obj_t)(&mp_thingz_display_raw_text_get_height_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_color), (mp_obj_t)(&mp_thingz_display_raw_text_get_color_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_show), (mp_obj_t)(&mp_thingz_display_raw_text_get_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_text), (mp_obj_t)(&mp_thingz_display_raw_text_get_text_obj)}

};

static MP_DEFINE_CONST_DICT(mp_thingz_display_raw_text_locals_dict, mp_thingz_display_raw_text_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_thingz_display_raw_text_type,
    MP_QSTR_Text,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_display_raw_text_make_new,
    locals_dict, &mp_thingz_display_raw_text_locals_dict
);