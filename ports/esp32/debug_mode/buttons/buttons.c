#include "buttons.h"

#include "debug_mode/debug_mode.h"

#include "common-thingz/thingz.h"
#include "common-thingz/thingz_button/thingz_button.h"
#include "common-thingz/thingz_button_touch/thingz_button_touch.h"

static char button_page_title[] = "Boutons";

void debug_mode_show_buttons_screen(void){
    char str[100];

    mp_map_t *locals_map_button = &(MP_OBJ_TYPE_GET_SLOT(&thingz_button_type, locals_dict)->map);
    mp_map_elem_t *is_pressed_elem = mp_map_lookup(locals_map_button, MP_OBJ_NEW_QSTR(MP_QSTR_is_pressed), MP_MAP_LOOKUP);
    
    mp_map_t *locals_map_button_touch = &(MP_OBJ_TYPE_GET_SLOT(&thingz_button_touch_type, locals_dict)->map);
    mp_map_elem_t *is_touched_elem = mp_map_lookup(locals_map_button_touch, MP_OBJ_NEW_QSTR(MP_QSTR_is_touched), MP_MAP_LOOKUP);
    
    if(is_pressed_elem == NULL)
        return;
    
    if(is_touched_elem == NULL)
        return;

    debug_mode_clear_screen();
    
    debug_mode_print_header();

    mp_fun_1_t is_pressed = ((mp_obj_fun_builtin_fixed_t*)is_pressed_elem->value)->fun._1;
    sprintf(str, "Bouton A: %s", mp_obj_get_int(is_pressed(&thingz_buttons[0])) ? "True" : "False");
    debug_mode_print_str(0, 1, str, 0, 0xffffff);
    sprintf(str, "Bouton B: %s", mp_obj_get_int(is_pressed(&thingz_buttons[1])) ? "True" : "False");
    debug_mode_print_str(0, 2, str, 0, 0xffffff);

    mp_fun_1_t is_touched = ((mp_obj_fun_builtin_fixed_t*)is_touched_elem->value)->fun._1;
    sprintf(str, "Bouton Haut: %s", mp_obj_get_int(is_touched(&thingz_touch_buttons[0])) ? "True" : "False");
    debug_mode_print_str(0, 3, str, 0, 0xffffff);
    sprintf(str, "Bouton Bas: %s", mp_obj_get_int(is_touched(&thingz_touch_buttons[1])) ? "True" : "False");
    debug_mode_print_str(0, 4, str, 0, 0xffffff);
    sprintf(str, "Bouton Gauche: %s", mp_obj_get_int(is_touched(&thingz_touch_buttons[3])) ? "True" : "False");
    debug_mode_print_str(0, 5, str, 0, 0xffffff);
    sprintf(str, "Bouton Droit: %s", mp_obj_get_int(is_touched(&thingz_touch_buttons[2])) ? "True" : "False");
    debug_mode_print_str(0, 6, str, 0, 0xffffff);

}

void debug_mode_buttons_enter(void){
    debug_mode_set_header_text(button_page_title);
}

void debug_mode_buttons_exit(void){

}