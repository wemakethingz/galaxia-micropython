#include "thingz_log.h"
#include "common-thingz/thingz_memory/thingz_memory.h"

#include "py/misc.h"
#include "py/mpprint.h"
#include "py/obj.h"
#include "py/objlist.h"
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objfun.h"

#include "py/bc.h"

#include "driver/gpio.h"
#include "esp_spiffs.h"


#include "common-thingz/thingz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* _search_columns(mp_obj_list_t* list, const char* column){
    for(int i = 0; i < list->len; i++){
        // mp_printf(MP_PYTHON_PRINTER, "search1\n");
        // mp_printf(MP_PYTHON_PRINTER, "%d %d %d\n", list->len, mp_obj_is_type(list->items[i], &mp_type_tuple), ((mp_obj_tuple_t*)list->items[i])->len);//, ((mp_obj_tuple_t*)list->items[i])->len, (mp_obj_tuple_t*)list->items[i]);
        if(mp_obj_is_type(list->items[i], &mp_type_tuple) && ((mp_obj_tuple_t*)list->items[i])->len == 2){
            // mp_printf(MP_PYTHON_PRINTER, "search2\n");
            mp_obj_t args[] = {
                ((mp_obj_tuple_t*)list->items[i])->items[0]
            };
            // mp_printf(MP_PYTHON_PRINTER, "search3\n");
            // mp_printf(MP_PYTHON_PRINTER, "%s %s\n", column, mp_obj_str_get_str(mp_type_str.make_new(&mp_type_str, 1, 0, args)));
            if(strcmp(mp_obj_str_get_str(MP_OBJ_TYPE_GET_SLOT(&mp_type_str,make_new)(&mp_type_str, 1, 0, args)), column) == 0){
                // mp_printf(MP_PYTHON_PRINTER, "search4\n");
                args[0] = ((mp_obj_tuple_t*)list->items[i])->items[1];
                // mp_printf(MP_PYTHON_PRINTER, "search5\n");
                return mp_obj_str_get_str(MP_OBJ_TYPE_GET_SLOT(&mp_type_str,make_new)(&mp_type_str, 1, 0, args));
            }
        }
    }
    return NULL;
}

void thingz_log_init(thingz_log_obj_t* log){

    log->base.type = &thingz_log_type;

    nvs_handle* nvsHandle;

    nvsHandle = thingz_memory_get_handle();
    if(nvs_get_u32(*nvsHandle, "log_r", (uint32_t*)&log->readPos) != ESP_OK){
        log->readPos = 0;
    }

    if(nvs_get_u32(*nvsHandle, "log_w", (uint32_t*)&log->writePos) != ESP_OK){
        log->writePos = 0;
    }
    

    log->columns = NULL; //mp_type_list.make_new(&mp_type_list, 0, 0, NULL);

}

void thingz_log_deinit(thingz_log_obj_t* log){
    
    if(log->columns){
        for(int i = 0; i < log->columns_len; i++){
            free(log->columns[i]);
        }
        free(log->columns);
    }
    log->columns_len = 0;
}


//NEW
static mp_obj_t mp_thingz_log_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    thingz_log_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_log_obj_t));
    self->base.type = &thingz_button_type;
    thingz_log_init(self);
    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_log_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_log_del_obj, mp_thingz_log_del);

// //ADD
static mp_obj_t mp_thingz_log_add(mp_obj_t self_in, mp_obj_list_t* list) {
	thingz_log_obj_t *self = MP_OBJ_TO_PTR(self_in);

    uint8_t found = 0;

    // return mp_const_none;

    if(self->columns == NULL){
        mp_raise_ValueError(MP_ERROR_TEXT("columns not set"));
    }
    
    if(mp_obj_is_type(list, &mp_type_list)){
        FILE* f = fopen("/spiffs/data.csv", "a");
        if(f){
            // mp_printf(MP_PYTHON_PRINTER, "search\n");
            // mp_printf(MP_PYTHON_PRINTER, "items %p %d\n", self->columns, self->columns_len);
            for(int i = 0; i < self->columns_len; i++){
                const char* c = _search_columns(list, self->columns[i]);
                // mp_printf(MP_PYTHON_PRINTER, "search end\n");
                if(i > 0)
                    fprintf(f, ";");
                if(c != NULL){
                    found++;
                    fprintf(f, "%s", c);
                }
            }
            fprintf(f, "\n");
            fclose(f);
        }else{
            mp_raise_OSError(MP_EFBIG);
        }
    }else{
        mp_raise_TypeError(MP_ERROR_TEXT("arg1: list expected"));
    }

    return mp_obj_new_int(found);
}

MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_log_add_obj, mp_thingz_log_add);

static mp_obj_t mp_thingz_log_delete(mp_obj_t self_in){
    thingz_log_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if(esp_spiffs_format("storage") == 0){
    // mp_printf(MP_PYTHON_PRINTER, "%d\n", remove("/spiffs/data.csv"));
        if(self->columns){
            FILE* f = fopen("/spiffs/data.csv", "w");
            if(f){
                for(int i = 0; i < self->columns_len; i++){
                    mp_obj_t args[] = {
                        self->columns[i]
                    };
                    fprintf(f, "%s", self->columns[i]);
                    if(i+1 < self->columns_len){
                        fprintf(f, ";");
                    }
                }
                fprintf(f, "\n");
                fclose(f);
            }else{
                mp_raise_OSError(errno);
            }
        }
        return mp_const_none;
    }else{
        mp_raise_OSError(errno);
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_log_delete_obj, mp_thingz_log_delete);

// SET COLUMNS
static mp_obj_t mp_thingz_log_set_columns(mp_obj_t self_in, mp_obj_list_t* list) {
	thingz_log_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if(!mp_obj_is_type(list, &mp_type_list)){
        mp_raise_TypeError(MP_ERROR_TEXT("arg1: list expected"));
    }
    if(self->columns){
        for(int i = 0; i < self->columns_len; i++){
            free(self->columns[i]);
        }
        free(self->columns);
        self->columns_len = 0;
    }
    self->columns_len = list->len;
    self->columns = malloc(sizeof(char*)*self->columns_len);
    for(int i = 0; i < self->columns_len; i++){
        self->columns[i] = malloc(strlen(mp_obj_str_get_str(list->items[i]))+1);
        strcpy(self->columns[i], mp_obj_str_get_str(list->items[i]));
    }
    // return mp_const_none;
    return mp_thingz_log_delete(self);
}

MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_log_set_columns_obj, mp_thingz_log_set_columns);

static const mp_map_elem_t thingz_log_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR_add), (mp_obj_t)&mp_thingz_log_add_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_delete), (mp_obj_t)&mp_thingz_log_delete_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_columns), (mp_obj_t)&mp_thingz_log_set_columns_obj },
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__), (mp_obj_t)&mp_thingz_log_del_obj },
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_log_local_dict,
	thingz_log_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_log_type,
    MP_QSTR_Log,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_log_make_new,
    locals_dict, &mp_thingz_log_local_dict
);