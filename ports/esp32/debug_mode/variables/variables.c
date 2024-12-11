#include <string.h>

#include "variables.h"

#include "debug_mode/debug_mode.h"

#include "py/mpstate.h"

static char variables_page_title[] = "Variables";

static bool _local_var_filter_key(const char* str){
    if(strcmp("__file__", str) == 0){
        return false;
    }
    if(strcmp("__name__", str) == 0){
        return false;
    }
    return true;
}

void debug_mode_show_local_var_screen(void){
    char tmp[100];
    uint8_t y=1;

    debug_mode_clear_screen();
    
    debug_mode_print_header();

    mp_obj_dict_t* dict = MP_STATE_THREAD(dict_locals);
    mp_map_t *map = NULL;
    
    map = mp_obj_dict_get_map(dict);
    if (map != NULL) {
        for (uint i = 0; i < map->alloc; i++) {
            tmp[0] = 0;
            if (map->table[i].key != MP_OBJ_NULL) {
                
                if (map->table[i].value != MP_OBJ_NULL) {
                    if(mp_obj_is_int(map->table[i].value)){
                        // ESP_LOGI(TAG, "VARIABLE - int: %s", mp_obj_str_get_str(map->table[i].key));
                        snprintf(tmp, 100, "%s: ", mp_obj_str_get_str(map->table[i].key));
                        mp_int_t r = mp_obj_get_int(map->table[i].value);
                        snprintf(tmp+strlen(tmp), 100-strlen(tmp), "%ld", r/*mp_obj_get_int(map->table[i].value)*/);
                    }else if(mp_obj_is_str(map->table[i].value)){
                        // ESP_LOGI(TAG, "VARIABLE - string");
                        const char* str = mp_obj_str_get_str(map->table[i].key);
                        if(_local_var_filter_key(str)){
                            snprintf(tmp, 100, "%s: ", mp_obj_str_get_str(map->table[i].key));
                            snprintf(tmp+strlen(tmp), 100-strlen(tmp), "%s", mp_obj_str_get_str(map->table[i].value));
                        }
                    }else if(mp_obj_is_float(map->table[i].value)){
                        // ESP_LOGI(TAG, "VARIABLE - float");
                        snprintf(tmp, 100, "%s: ", mp_obj_str_get_str(map->table[i].key));
                        snprintf(tmp+strlen(tmp), 100-strlen(tmp), "%f", (double)mp_obj_get_float(map->table[i].value));
                    }else if(mp_obj_is_bool(map->table[i].value)){
                        // ESP_LOGI(TAG, "VARIABLE - bool");
                        snprintf(tmp, 100, "%s: ", mp_obj_str_get_str(map->table[i].key));
                        snprintf(tmp+strlen(tmp), 100-strlen(tmp), "%s", mp_obj_get_int(map->table[i].value) ? "True" : "False");
                    }else{
                        // ESP_LOGI(TAG, "VARIABLE - not supported");
                        // mp_obj_type_t *type = mp_obj_get_type(map->table[i].value);
                        // snprintf(tmp+strlen(tmp), 50-strlen(tmp), "%s", qstr_str(type->name)); 
                    }
                }

                if(strlen(tmp) > 0){
                    debug_mode_print_str(0, y, tmp, 0, 0xffffff);
                    y++;
                }
            
            }
        }
    }
}


void debug_mode_local_var_enter(void){
    debug_mode_set_header_text(variables_page_title);
}

void debug_mode_local_var_exit(void){
    
}