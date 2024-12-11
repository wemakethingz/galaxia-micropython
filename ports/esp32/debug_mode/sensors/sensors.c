#include <string.h>

#include "sensors.h"

#include "debug_mode/debug_mode.h"

#include "py/objlist.h"
#include "py/objmodule.h"

#include "common-thingz/thingz.h"


#include "common-thingz/thingz_led/thingz_led.h"
#include "common-thingz/thingz_accel/thingz_accel.h"


static char sensors_page_title[] = "Capteurs";

void debug_mode_show_sensors_screen(void){
    char str[256];
    uint8_t pos[8];
    double accel_compass[6] = {0};
    int8_t temperature = 0;

    mp_map_t *locals_map_sensor = &(MP_OBJ_TYPE_GET_SLOT(&thingz_led_type,locals_dict)->map);
    mp_map_elem_t *sensor_elem = mp_map_lookup(locals_map_sensor, MP_OBJ_NEW_QSTR(MP_QSTR_read_light_level), MP_MAP_LOOKUP);

    if(sensor_elem == NULL)
        return;
    mp_printf(MP_PYTHON_PRINTER, "light\n");
    mp_fun_1_t sensor_func = ((mp_obj_fun_builtin_fixed_t*)sensor_elem->value)->fun._1;
    mp_obj_list_t* list;
    int lum = mp_obj_get_int(sensor_func(&thingz_led));


    locals_map_sensor = &(MP_OBJ_TYPE_GET_SLOT(&thingz_accel_type, locals_dict)->map);
    sensor_elem = mp_map_lookup(locals_map_sensor, MP_OBJ_NEW_QSTR(MP_QSTR_get_values), MP_MAP_LOOKUP);
    
    if(sensor_elem == NULL)
        return;
    mp_printf(MP_PYTHON_PRINTER, "accel\n");
    sensor_func = ((mp_obj_fun_builtin_fixed_t*)sensor_elem->value)->fun._1;
    list = (mp_obj_list_t*)sensor_func(&thingz_accel);

    accel_compass[0] = mp_obj_get_float(list->items[0]);
    accel_compass[1] = mp_obj_get_float(list->items[1]);
    accel_compass[2] = mp_obj_get_float(list->items[2]);

    locals_map_sensor = &(MP_OBJ_TYPE_GET_SLOT(&thingz_compass_type,locals_dict)->map);
    sensor_elem = mp_map_lookup(locals_map_sensor, MP_OBJ_NEW_QSTR(MP_QSTR_get_values), MP_MAP_LOOKUP);
    
    if(sensor_elem == NULL)
        return;
    mp_printf(MP_PYTHON_PRINTER, "magne\n");
    sensor_func = ((mp_obj_fun_builtin_fixed_t*)sensor_elem->value)->fun._1;
    
    // common_hal_displayio_display_refresh(&displays[0].display, 10000, 10000);
    list = (mp_obj_list_t*)sensor_func(&thingz_compass);

    accel_compass[3] = mp_obj_get_float(list->items[0]);
    accel_compass[4] = mp_obj_get_float(list->items[1]);
    accel_compass[5] = mp_obj_get_float(list->items[2]);

    // accel_compass[3] = 0;
    // accel_compass[4] = 0;
    // accel_compass[5] = 0;

    mp_obj_module_t* thingz_module =  mp_module_get_builtin(MP_QSTR_thingz, false);
    locals_map_sensor = &(thingz_module->globals->map);
    sensor_elem = mp_map_lookup(locals_map_sensor, MP_OBJ_NEW_QSTR(MP_QSTR_temperature), MP_MAP_LOOKUP);
    
    if(sensor_elem == NULL)
        return;
    mp_printf(MP_PYTHON_PRINTER, "temp\n");
    mp_fun_0_t temp_func = ((mp_obj_fun_builtin_fixed_t*)sensor_elem->value)->fun._0;

    temperature = mp_obj_get_int(temp_func());

    debug_mode_clear_screen();
    
    debug_mode_print_header();

    sprintf(str, "Lumière: ");
    debug_mode_print_str(0, 1, str, 0, 0xffffff);
    pos[6] = strlen(str);  
    sprintf(str, "Accel X: ");
    debug_mode_print_str(0, 2, str, 0, 0xffffff);
    pos[0] = strlen(str);  
    sprintf(str, "Accel Y: ");
    debug_mode_print_str(0, 3, str, 0, 0xffffff);
    pos[1] = strlen(str);  
    sprintf(str, "Accel Z: ");
    debug_mode_print_str(0, 4, str, 0, 0xffffff);
    pos[2] = strlen(str);  
    sprintf(str, "Magneto X: ");
    debug_mode_print_str(0, 5, str, 0, 0xffffff);
    pos[3] = strlen(str);  
    sprintf(str, "Magneto Y: ");
    debug_mode_print_str(0, 6, str, 0, 0xffffff);
    pos[4] = strlen(str);  
    sprintf(str, "Magneto Z: ");
    debug_mode_print_str(0, 7, str, 0, 0xffffff);
    pos[5] = strlen(str);   

    sprintf(str, "%d", lum);
    debug_mode_print_str(pos[6], 1, str, 0, 0xffffff);  
// common_hal_displayio_display_refresh(&displays[0].display, 10000, 10000);

    sprintf(str, "Temperature: ");
    debug_mode_print_str(0, 8, str, 0, 0xffffff);
    pos[7] = strlen(str);

    // common_hal_displayio_display_refresh(&displays[0].display, 10000, 10000);
    sprintf(str, "%.2f mg", accel_compass[0]);
    debug_mode_print_str(pos[0], 2, str, 0, 0xffffff);
    sprintf(str, "%.2f mg", accel_compass[1]);
    debug_mode_print_str(pos[1], 3, str, 0, 0xffffff);
    sprintf(str, "%.2f mg", accel_compass[2]);
    debug_mode_print_str(pos[2], 4, str, 0, 0xffffff);
    // common_hal_displayio_display_refresh(&displays[0].display, 10000, 10000);

    sprintf(str, "%.2f uT", accel_compass[3]);
    debug_mode_print_str(pos[3], 5, str, 0, 0xffffff);
    sprintf(str, "%.2f uT", accel_compass[4]);
    debug_mode_print_str(pos[4], 6, str, 0, 0xffffff);
    sprintf(str, "%.2f uT", accel_compass[5]);
    debug_mode_print_str(pos[5], 7, str, 0, 0xffffff);

    sprintf(str, "%d C", temperature);
    debug_mode_print_str(pos[7], 8, str, 0, 0xffffff);

}

void debug_mode_sensors_enter(void){
    debug_mode_set_header_text(sensors_page_title);
}

void debug_mode_sensors_exit(void){
    
}