#include "led.h"

#include "debug_mode/debug_mode.h"
#include "common-thingz/thingz.h"

static char led_page_title[] = "Led";


void debug_mode_show_led_screen(void){
     char str[100];

    mp_map_t *locals_map_led = &(MP_OBJ_TYPE_GET_SLOT(&thingz_led_type, locals_dict)->map);
    mp_map_elem_t *get_color_elem = mp_map_lookup(locals_map_led, MP_OBJ_NEW_QSTR(MP_QSTR_get_red), MP_MAP_LOOKUP);
    
    if(get_color_elem == NULL)
        return;

    mp_fun_1_t get_color = ((mp_obj_fun_builtin_fixed_t*)get_color_elem->value)->fun._1;

    debug_mode_clear_screen();
    
    debug_mode_print_header();

    sprintf(str, "Led rouge: %ld", mp_obj_get_int(get_color(&thingz_led)));
    debug_mode_print_str(0, 1, str, 0, 0xffffff);

    get_color_elem = mp_map_lookup(locals_map_led, MP_OBJ_NEW_QSTR(MP_QSTR_get_green), MP_MAP_LOOKUP);
    
    if(get_color_elem == NULL)
        return;

    get_color = ((mp_obj_fun_builtin_fixed_t*)get_color_elem->value)->fun._1;

     sprintf(str, "Led verte: %ld", mp_obj_get_int(get_color(&thingz_led)));
    debug_mode_print_str(0, 2, str, 0, 0xffffff);

    get_color_elem = mp_map_lookup(locals_map_led, MP_OBJ_NEW_QSTR(MP_QSTR_get_blue), MP_MAP_LOOKUP);
    
    if(get_color_elem == NULL)
        return;

    get_color = ((mp_obj_fun_builtin_fixed_t*)get_color_elem->value)->fun._1;

    sprintf(str, "Led bleue: %ld", mp_obj_get_int(get_color(&thingz_led)));
    debug_mode_print_str(0, 3, str, 0, 0xffffff);
}

void debug_mode_led_enter(void){
    debug_mode_set_header_text(led_page_title);
}

void debug_mode_led_exit(void){
    
}