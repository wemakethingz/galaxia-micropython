#include "common-thingz/thingz_screen/thingz_screen_raw.h"
#include "py/obj.h"
#include "common-thingz/thingz_display/Raw/thingz_display_raw.h"
#include "common-thingz/thingz_display/Raw/thingz_display_raw_image.h"
#include "common-thingz/thingz_display/Raw/thingz_display_raw_rectangle.h"
#include "common-thingz/thingz_display/Raw/thingz_display_raw_text.h"
#include "common-thingz/thingz_screen/thingz_screen.h"


#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objint.h"
#include "py/mpz.h"
#include <string.h>


void thingz_display_raw_init(thingz_display_raw_obj_t* raw){
    raw->base.type = &thingz_display_raw_type;
}

void thingz_display_raw_deinit(thingz_display_raw_obj_t* raw){
    thingz_screen_raw_remove_show_obj(&(thingz_screen.raw), NULL);
}

//|
//| """ Thingz Display Raw
//| """
//|
//| class Raw:
//|    """
//|    Use the LCD to display graphical elements
//|    """
//|

//|    Img: Img
//|    """
//|    Class `Img` to create an image
//|    """
//|
//|    Rect: Rect
//|    """
//|    Class `Rect` to create a rectangle
//|    """
//|
//|    Text: Text
//|    """
//|    Class `Text` to create a text
//|    """
//NEW
static mp_obj_t mp_thingz_display_raw_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_display_raw_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_display_raw_obj_t));
    
    self->base.type = &thingz_display_raw_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_display_raw_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_del_obj, mp_thingz_display_raw_del);


//SHOW
//|    def show(self) -> None:
//|        """
//|        Show the raw interface
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_show(mp_obj_t self_in){
    thingz_screen_switch_mode(COMMON_THINGZ_SCREEN_MODE_RAW);
    
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_display_raw_show_obj, mp_thingz_display_raw_show);

// Helper function to parse coordinate (int or float)
static inline int32_t parse_coordinate(mp_obj_t obj, const char* arg_name) {
    if(mp_obj_is_int(obj)){
        return mp_obj_get_int(obj);
    }
    const mp_obj_type_t* type = mp_obj_get_type(obj);
    if(type == &mp_type_float){
        return (int32_t)mp_obj_get_float(obj);
    }
    if(type == &mp_type_int){
        mp_uint_t mpint;
        mpz_t mp = ((mp_obj_int_t*)obj)->mpz;
        if(mpz_as_uint_checked(&mp, &mpint)){
            return (int32_t)mpint;
        }
        mp_raise_TypeError(arg_name);
    }
    return 0;
}

//|    def print(self, x:int, y:int, txt:str) -> None:
//|        """
//|        Print text at a given position
//|
//|        :param int x: X position
//|        :param int y: Y position
//|        :param str txt: The text to print
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_print(size_t n_pos_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    int32_t x = parse_coordinate(pos_args[1], "argument 'x': cannot get long");
    int32_t y = parse_coordinate(pos_args[2], "argument 'y': cannot get long");
    const char* s = mp_obj_str_get_str(pos_args[3]);
    thingz_screen_raw_write(&(thingz_screen.raw), x, y, s, strlen(s), 0xffffffff);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_display_raw_print_obj, 4, mp_thingz_display_raw_print);

//|    def print_bmp(self, x:int, y:int, path:str) -> None:
//|        """
//|        Print BMP file at a given position
//|
//|        :param int x: X position
//|        :param int y: Y position
//|        :param str path: The path to the BMP file
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_display_raw_print_bmp(size_t n_pos_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    int32_t x = parse_coordinate(pos_args[1], "argument 'x': cannot get long");
    int32_t y = parse_coordinate(pos_args[2], "argument 'y': cannot get long");
    const char* s = mp_obj_str_get_str(pos_args[3]);
    thingz_screen_raw_print_bmp(&(thingz_screen.raw), x, y, s, 0xFFFFFFFF, 1);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_display_raw_print_bmp_obj, 4, mp_thingz_display_raw_print_bmp);

static const mp_rom_map_elem_t thingz_display_raw_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__),     (mp_obj_t)&mp_thingz_display_raw_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_show),        (mp_obj_t)&mp_thingz_display_raw_show_obj },  
    { MP_OBJ_NEW_QSTR(MP_QSTR_print),       (mp_obj_t)&mp_thingz_display_raw_print_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_print_bmp),   (mp_obj_t)&mp_thingz_display_raw_print_bmp_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Img),         (mp_obj_t)&mp_thingz_display_raw_img_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Rect),        (mp_obj_t)&mp_thingz_display_raw_rectangle_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Text),        (mp_obj_t)&mp_thingz_display_raw_text_type },
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_display_raw_local_dict,
	thingz_display_raw_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_display_raw_type,
    MP_QSTR_Raw,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_display_raw_make_new,
    locals_dict, &mp_thingz_display_raw_local_dict
);