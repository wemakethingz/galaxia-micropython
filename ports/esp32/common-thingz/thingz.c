#include "thingz.h"
#include "thingz_memory/thingz_memory.h"

#include <string.h>

#include "esp_log.h"

#include "driver/temperature_sensor.h"

static const char *TAG = "THINGZ";

#if MICROPY_THINGZ_BUTTONS_NB
    thingz_button_obj_t thingz_buttons[MICROPY_THINGZ_BUTTONS_NB];
    uint8_t thingz_buttons_ios[MICROPY_THINGZ_BUTTONS_NB] = MICROPY_THINGZ_BUTTONS_IOS;
#endif

#if MICROPY_THINGZ_TOUCH_BUTTONS_NB
    thingz_button_touch_obj_t thingz_touch_buttons[MICROPY_THINGZ_TOUCH_BUTTONS_NB];
    uint8_t thingz_touch_buttons_ios[MICROPY_THINGZ_TOUCH_BUTTONS_NB] = MICROPY_THINGZ_TOUCH_BUTTONS_IOS;
#endif


#if MICROPY_THINGZ_ACCEL
    thingz_accel_obj_t thingz_accel;
#endif

#if MICROPY_THINGZ_MAGNETO
    thingz_compass_obj_t thingz_compass;
#endif

#if MICROPY_THINGZ_LED
    thingz_led_obj_t thingz_led;
#endif
#if MICROPY_THINGZ_SOUND
    thingz_sound_obj_t thingz_sound;
#endif
#if MICROPY_THINGZ_RADIO
    thingz_radio_obj_t thingz_radio;
#endif

#if MICROPY_THINGZ_LOG
    thingz_log_obj_t thingz_log;
#endif
// thingz_button_touch_obj_t thingz_button_n;
// thingz_button_touch_obj_t thingz_button_s;
// thingz_button_touch_obj_t thingz_button_e;
// thingz_button_touch_obj_t thingz_button_o;

// thingz_led_obj_t thingz_led;

// thingz_accel_obj_t thingz_accel;

// thingz_compass_obj_t thingz_compass;

// thingz_temp_obj_t thingz_temp;

// thingz_sound_obj_t thingz_sound;

// thingz_microphone_obj_t thingz_microphone;

// thingz_radio_obj_t thingz_radio;
#if MICROPY_THINGZ_SCREEN
    thingz_display_obj_t thingz_display;
#endif

static float thingz_temp_offset = 9;

static uint8_t thingz_input_target;
static char python_file_to_exec[256];

static temperature_sensor_handle_t temperature_handle;

void (*thingz_input_event_cb)(thingz_input_event_t);

void thingz_set_python_file_to_exec(const char* name){
    nvs_handle* nvsHandle;
    nvsHandle = thingz_memory_get_handle();
    nvs_set_str(*nvsHandle, "filename", name);
    thingz_print_filename(name);
}

char* thingz_get_python_file_to_exec(uint8_t cp437, uint8_t print){
    nvs_handle* nvsHandle;
    nvsHandle = thingz_memory_get_handle();
    size_t length = 256;
    if(nvs_get_str(*nvsHandle, "filename", python_file_to_exec, &length) != ESP_OK){
        sprintf(python_file_to_exec, "%s", "main.py");
    }
    
    if(print)
        thingz_print_filename(python_file_to_exec);


    if(cp437){
        char str[50];
        length = strlen(python_file_to_exec);
        uint8_t count = 0;

        const byte* i = (byte*) python_file_to_exec;
        while( i < (byte*)(python_file_to_exec+length) && count < 50){
            unichar c = utf8_get_char(i);
            //printf("char %d\n", c);
            i = utf8_next_char(i);
            //mp_printf(MP_PYTHON_PRINTER, "%d\n", c);
            if(c == 0x00C7){
            str[count] = 0x80;
            }
            else if(c == 0x00FC){
            str[count] = 0x81;
            }
            else if(c == 0x00E9){
            str[count] = 0x82;
            }
            else if(c == 0x00E2){
            str[count] = 0x83;
            }
            else if(c == 0x00E4){
            str[count] = 0x84;
            }
            else if(c == 0x00E0){
            str[count] = 0x85;
            }
            else if(c == 0x00E5){
            str[count] = 0x86;
            }
            else if(c == 0x00E7){
            str[count] = 0x87;
            }
            else if(c == 0x00EA){
            str[count] = 0x88;
            }
            else if(c == 0x00EB){
            str[count] = 0x89;
            }
            else if(c == 0x00E8){
            str[count] = 0x8A;
            }
            else if(c == 0x00EF){
            str[count] = 0x8B;
            }
            else if(c == 0x00EE){
            str[count] = 0x8C;
            }
            else if(c == 0x00EC){
            str[count] = 0x8D;
            }
            else if(c == 0x00C4){
            str[count] = 0x8E;
            }
            else if(c == 0x00C5){
            str[count] = 0x8F;
            }
            else if(c == 0x00C9){
            str[count] = 0x90;
            }
            else if(c == 0x00E6){
            str[count] = 0x91;
            }
            else if(c == 0x00C6){
            str[count] = 0x92;
            }
            else if(c == 0x00F4){
            str[count] = 0x93;
            }
            else if(c == 0x00F6){
            str[count] = 0x94;
            }
            else if(c == 0x00F2){
            str[count] = 0x95;
            }
            else if(c == 0x00FB){
            str[count] = 0x96;
            }
            else if(c == 0x00F9){
            str[count] = 0x97;
            }
            else if(c == 0x00FF){
            str[count] = 0x98;
            }
            else if(c == 0x00D6){
            str[count] = 0x99;
            }
            else if(c == 0x00DC){
            str[count] = 0x9A;
            }
            else if(c == 0x00A2){
            str[count] = 0x9B;
            }
            else if(c == 0x00A3){
            str[count] = 0x9C;
            }
            else if(c == 0x00A5){
            str[count] = 0x9D;
            }
            else if(c == 0x20A7){
            str[count] = 0x9E;
            }
            else if(c == 0x0192){
            str[count] = 0x9F;
            }
            else if(c == 0x00E1){
            str[count] = 0xA0;
            }
            else if(c == 0x00ED){
            str[count] = 0xA1;
            }
            else if(c == 0x00F3){
            str[count] = 0xA2;
            }
            else if(c == 0x00FA){
            str[count] = 0xA3;
            }
            else if(c == 0x00F1){
            str[count] = 0xA4;
            }
            else if(c == 0x00D1){
            str[count] = 0xA5;
            }
            else if(c == 0x00AA){
            str[count] = 0xA6;
            }
            else if(c == 0x00BA){
            str[count] = 0xA7;
            }
            else if(c == 0x00BF){
            str[count] = 0xA8;
            }
            else if(c == 0x2310){
            str[count] = 0xA9;
            }
            else if(c == 0x00AC){
            str[count] = 0xAA;
            }
            else if(c == 0x00BD){
            str[count] = 0xAB;
            }
            else if(c == 0x00BC){
            str[count] = 0xAC;
            }
            else if(c == 0x00A1){
            str[count] = 0xAD;
            }
            else if(c == 0x00AB){
            str[count] = 0xAE;
            }
            else if(c == 0x00BB){
            str[count] = 0xAF;
            }
            else if(c == 0x2591){
            str[count] = 0xB0;
            }
            else if(c == 0x2592){
            str[count] = 0xB1;
            }
            else if(c == 0x2593){
            str[count] = 0xB2;
            }
            else if(c == 0x2502){
            str[count] = 0xB3;
            }
            else if(c == 0x2524){
            str[count] = 0xB4;
            }
            else if(c == 0x2561){
            str[count] = 0xB5;
            }
            else if(c == 0x2562){
            str[count] = 0xB6;
            }
            else if(c == 0x2556){
            str[count] = 0xB7;
            }
            else if(c == 0x2555){
            str[count] = 0xB8;
            }
            else if(c == 0x2563){
            str[count] = 0xB9;
            }
            else if(c == 0x2551){
            str[count] = 0xBA;
            }
            else if(c == 0x2557){
            str[count] = 0xBB;
            }
            else if(c == 0x255D){
            str[count] = 0xBC;
            }
            else if(c == 0x255C){
            str[count] = 0xBD;
            }
            else if(c == 0x255B){
            str[count] = 0xBE;
            }
            else if(c == 0x2510){
            str[count] = 0xBF;
            }
            else if(c == 0x2514){
            str[count] = 0xC0;
            }
            else if(c == 0x2534){
            str[count] = 0xC1;
            }
            else if(c == 0x252C){
            str[count] = 0xC2;
            }
            else if(c == 0x251C){
            str[count] = 0xC3;
            }
            else if(c == 0x2500){
            str[count] = 0xC4;
            }
            else if(c == 0x253C){
            str[count] = 0xC5;
            }
            else if(c == 0x255E){
            str[count] = 0xC6;
            }
            else if(c == 0x255F){
            str[count] = 0xC7;
            }
            else if(c == 0x255A){
            str[count] = 0xC8;
            }
            else if(c == 0x2554){
            str[count] = 0xC9;
            }
            else if(c == 0x2569){
            str[count] = 0xCA;
            }
            else if(c == 0x2566){
            str[count] = 0xCB;
            }
            else if(c == 0x2560){
            str[count] = 0xCC;
            }
            else if(c == 0x2550){
            str[count] = 0xCD;
            }
            else if(c == 0x256C){
            str[count] = 0xCE;
            }
            else if(c == 0x2567){
            str[count] = 0xCF;
            }
            else if(c == 0x2568){
            str[count] = 0xD0;
            }
            else if(c == 0x2564){
            str[count] = 0xD1;
            }
            else if(c == 0x2565){
            str[count] = 0xD2;
            }
            else if(c == 0x2559){
            str[count] = 0xD3;
            }
            else if(c == 0x2558){
            str[count] = 0xD4;
            }
            else if(c == 0x2552){
            str[count] = 0xD5;
            }
            else if(c == 0x2553){
            str[count] = 0xD6;
            }
            else if(c == 0x256B){
            str[count] = 0xD7;
            }
            else if(c == 0x256A){
            str[count] = 0xD8;
            }
            else if(c == 0x2518){
            str[count] = 0xD9;
            }
            else if(c == 0x250C){
            str[count] = 0xDA;
            }
            else if(c == 0x2588){
            str[count] = 0xDB;
            }
            else if(c == 0x2584){
            str[count] = 0xDC;
            }
            else if(c == 0x258C){
            str[count] = 0xDD;
            }
            else if(c == 0x2590){
            str[count] = 0xDE;
            }
            else if(c == 0x2580){
            str[count] = 0xDF;
            }
            else if(c == 0x03B1){
            str[count] = 0xE0;
            }
            else if(c == 0x00DF){
            str[count] = 0xE1;
            }
            else if(c == 0x0393){
            str[count] = 0xE2;
            }
            else if(c == 0x03C0){
            str[count] = 0xE3;
            }
            else if(c == 0x03A3){
            str[count] = 0xE4;
            }
            else if(c == 0x03C3){
            str[count] = 0xE5;
            }
            else if(c == 0x00B5){
            str[count] = 0xE6;
            }
            else if(c == 0x03C4){
            str[count] = 0xE7;
            }
            else if(c == 0x03A6){
            str[count] = 0xE8;
            }
            else if(c == 0x0398){
            str[count] = 0xE9;
            }
            else if(c == 0x03A9){
            str[count] = 0xEA;
            }
            else if(c == 0x03B4){
            str[count] = 0xEB;
            }
            else if(c == 0x221E){
            str[count] = 0xEC;
            }
            else if(c == 0x03C6){
            str[count] = 0xED;
            }
            else if(c == 0x03B5){
            str[count] = 0xEE;
            }
            else if(c == 0x2229){
            str[count] = 0xEF;
            }
            else if(c == 0x2261){
            str[count] = 0xF0;
            }
            else if(c == 0x00B1){
            str[count] = 0xF1;
            }
            else if(c == 0x2265){
            str[count] = 0xF2;
            }
            else if(c == 0x2264){
            str[count] = 0xF3;
            }
            else if(c == 0x2320){
            str[count] = 0xF4;
            }
            else if(c == 0x2321){
            str[count] = 0xF5;
            }
            else if(c == 0x00F7){
            str[count] = 0xF6;
            }
            else if(c == 0x2248){
            str[count] = 0xF7;
            }
            else if(c == 0x00B0){
            str[count] = 0xF8;
            }
            else if(c == 0x2219){
            str[count] = 0xF9;
            }
            else if(c == 0x00B7){
            str[count] = 0xFA;
            }
            else if(c == 0x221A){
            str[count] = 0xFB;
            }
            else if(c == 0x207F){
            str[count] = 0xFC;
            }
            else if(c == 0x00B2){
            str[count] = 0xFD;
            }
            else if(c == 0x25A0){
            str[count] = 0xFE;
            }
            else if(c == 0x00A0){
            str[count] = 0xFF;
            }else{
                str[count] = (char)c;
            }
            count++;
        }
        str[count] = 0;

        length = strlen(str);
        for(size_t j = 0 ; j < length; j++){
            python_file_to_exec[j] = str[j];
        }
        python_file_to_exec[length] = 0;
        //mp_printf(MP_PYTHON_PRINTER, "%s\n", python_file_to_exec);
    }
    return python_file_to_exec;
}

uint8_t thingz_get_input_target(void){
    return thingz_input_target;
}

void thingz_set_input_target(uint8_t target, void (*callback)(thingz_input_event_t)){
    thingz_input_target = target;
    thingz_input_event_cb = callback;
}

input_event_callback thingz_get_input_event_callback(void){
    return thingz_input_event_cb;
}

void thingz_print_filename(const char* name){

    thingz_screen_print_header(name);
    // uint32_t length = strlen(name);
    // uint8_t count = 0;

    
    // const byte* i = (byte*) name;
    // while( i < (byte*)(name+length) && count < 25){
    //     unichar c = utf8_get_char(i);
    //     //printf("char %d\n", c);
    //     i = utf8_next_char(i);
    
    //     if (c < 128) {
    //         if (c >= 0x20 && c <= 0x7e) {
    //             uint8_t index = fontio_builtinfont_get_glyph_index(MP_OBJ_FROM_PTR(&supervisor_terminal_font), c);
    //             // common_hal_displayio_tilegrid_set_tile(&thingz_filename_text_tilegrid, count, 0, index);
    //         }else if(c == '\n'){

    //         }else if(c == '\r'){
    //             count=0;

    //         }else if(c == '\t'){
    //             uint8_t index = fontio_builtinfont_get_glyph_index(MP_OBJ_FROM_PTR(&supervisor_terminal_font), ' ');
    //             // common_hal_displayio_tilegrid_set_tile(&thingz_filename_text_tilegrid, count, 0, index);
    //         }
    //     }else{
    //         uint8_t index = fontio_builtinfont_get_glyph_index(MP_OBJ_FROM_PTR(&supervisor_terminal_font), c);
    //         // if(c == 0x82)
    //         //     index = fontio_builtinfont_get_glyph_index(MP_OBJ_FROM_PTR(&supervisor_terminal_font), utf8_get_char((byte*)"é"));
    //         // if(index != 0xff && index < thingz_filename_text_tilegrid.tiles_in_bitmap){
    //         //     // common_hal_displayio_tilegrid_set_tile(&thingz_filename_text_tilegrid, count, 0, index);
    //         // }else{
    //         //     index = fontio_builtinfont_get_glyph_index(MP_OBJ_FROM_PTR(&supervisor_terminal_font), ' ');
    //         //     // common_hal_displayio_tilegrid_set_tile(&thingz_filename_text_tilegrid, count, 0, index);
    //         // }
    //     }
    //     count++;
    // }
    // for(int j = count; j < 25; j++){
    //     // common_hal_displayio_tilegrid_set_tile(&thingz_filename_text_tilegrid, count, 0, fontio_builtinfont_get_glyph_index(MP_OBJ_FROM_PTR(&supervisor_terminal_font), ' '));
    //     count++;
    // }
    // // common_hal_displayio_display_refresh(&displays[0].display, 0xffffffff, 10000);
}


//GET TEMP
static mp_obj_t mp_thingz_get_temp(void) {
    //float temp = thingz_accel_compass_temp_get_temp(&thingz_temp);
	float tsens_out;

	temperature_sensor_get_celsius(temperature_handle, &tsens_out);
	return mp_obj_new_int(tsens_out- thingz_temp_offset);
}

MP_DEFINE_CONST_FUN_OBJ_0(mp_thingz_get_temp_obj, mp_thingz_get_temp);


//SET TEMPERATURE OFFSET
static mp_obj_t mp_thingz_set_temperature_offset(mp_obj_t offset) {

	float off;
	if(mp_obj_is_int(offset)){
		off = (float)mp_obj_get_int(offset);
	}else{
		off = mp_obj_get_float(offset);
	}
	
	thingz_temp_offset = off;

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_set_temperature_offset_obj, mp_thingz_set_temperature_offset);

static const mp_map_elem_t thingz_module_globals_table[] = {
	{ MP_ROM_QSTR(MP_QSTR___name__), 				MP_ROM_QSTR(MP_QSTR_thingz) },
	#if MICROPY_THINGZ_BUTTONS_NB
    MICROPY_THINGZ_BUTTONS(),
    #endif
    #if MICROPY_THINGZ_TOUCH_BUTTONS_NB
    MICROPY_THINGZ_TOUCH_BUTTONS(),
    #endif
    #if MICROPY_THINGZ_SCREEN
    { MP_ROM_QSTR(MP_QSTR_display), MP_ROM_PTR(&thingz_display)},
    #endif
    #if MICROPY_THINGZ_ACCEL
    { MP_ROM_QSTR(MP_QSTR_accelerometer),			MP_ROM_PTR(&thingz_accel)},
    #endif
    #if MICROPY_THINGZ_MAGNETO
    { MP_ROM_QSTR(MP_QSTR_compass),					MP_ROM_PTR(&thingz_compass)},
    #endif
    #if MICROPY_THINGZ_LED
    { MP_ROM_QSTR(MP_QSTR_led),  					MP_ROM_PTR(&thingz_led)},
    #endif
    #if MICROPY_THINGZ_SOUND
    { MP_ROM_QSTR(MP_QSTR_sound),					MP_ROM_PTR(&thingz_sound)},
    #endif
    #if MICROPY_THINGZ_RADIO
    { MP_ROM_QSTR(MP_QSTR_radio),					MP_ROM_PTR(&thingz_radio)},
    #endif
    #if MICROPY_THINGZ_LOG
    { MP_ROM_QSTR(MP_QSTR_log),					    MP_ROM_PTR(&thingz_log)},
    #endif
	// { MP_ROM_QSTR(MP_QSTR_button_b), 				MP_ROM_PTR(&thingz_button_b)},
	// { MP_ROM_QSTR(MP_QSTR_touch_n), 				MP_ROM_PTR(&thingz_button_n)},
	// { MP_ROM_QSTR(MP_QSTR_touch_s), 				MP_ROM_PTR(&thingz_button_s)},
	// { MP_ROM_QSTR(MP_QSTR_touch_e), 				MP_ROM_PTR(&thingz_button_e)},
	// { MP_ROM_QSTR(MP_QSTR_touch_w), 				MP_ROM_PTR(&thingz_button_o)},
	// { MP_ROM_QSTR(MP_QSTR_led),  					MP_ROM_PTR(&thingz_led)},
	// { MP_ROM_QSTR(MP_QSTR_accelerometer),			MP_ROM_PTR(&thingz_accel)},
	// { MP_ROM_QSTR(MP_QSTR_compass),					MP_ROM_PTR(&thingz_compass)},
	// { MP_ROM_QSTR(MP_QSTR_sound),					MP_ROM_PTR(&thingz_sound)},
	// { MP_ROM_QSTR(MP_QSTR_radio),					MP_ROM_PTR(&thingz_radio)},
	{ MP_ROM_QSTR(MP_QSTR_temperature),   			(mp_obj_t)(&mp_thingz_get_temp_obj)},
	{ MP_ROM_QSTR(MP_QSTR_set_temperature_offset),  (mp_obj_t)(&mp_thingz_set_temperature_offset_obj)},
	// { MP_ROM_QSTR(MP_QSTR_display),					MP_ROM_PTR(&thingz_display)},
	// // { MP_ROM_QSTR(MP_QSTR_microphone),				MP_ROM_PTR(&thingz_microphone)},

	// { MP_ROM_QSTR(MP_QSTR_pin0), (mp_obj_t)&microbit_p0_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin1), (mp_obj_t)&microbit_p1_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin2), (mp_obj_t)&microbit_p2_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin3), (mp_obj_t)&microbit_p3_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin4), (mp_obj_t)&microbit_p4_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin5), (mp_obj_t)&microbit_p5_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin6), (mp_obj_t)&microbit_p6_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin7), (mp_obj_t)&microbit_p7_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin8), (mp_obj_t)&microbit_p8_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin9), (mp_obj_t)&microbit_p9_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin10), (mp_obj_t)&microbit_p10_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin11), (mp_obj_t)&microbit_p11_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin12), (mp_obj_t)&microbit_p12_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin13), (mp_obj_t)&microbit_p13_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin14), (mp_obj_t)&microbit_p14_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin15), (mp_obj_t)&microbit_p15_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin16), (mp_obj_t)&microbit_p16_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin19), (mp_obj_t)&microbit_p19_obj },
    // { MP_ROM_QSTR(MP_QSTR_pin20), (mp_obj_t)&microbit_p20_obj },
};

static MP_DEFINE_CONST_DICT (
	thingz_module_globals,
	thingz_module_globals_table
);

const mp_obj_module_t thingz_module = {
.base = { &mp_type_module },
.globals = (mp_obj_dict_t*)&thingz_module_globals,
};


MP_REGISTER_MODULE(MP_QSTR_thingz, thingz_module);

void thingz_init(void){

    ESP_LOGE(TAG, "thingz init start heap 8bit %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGE(TAG, "thingz init start heap 32bit %d", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    ESP_LOGE(TAG, "thingz init start heap default %d", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    int i;
	#if MICROPY_THINGZ_BUTTONS_NB
    for(i = 0; i < MICROPY_THINGZ_BUTTONS_NB; i++){
        thingz_button_init(&thingz_buttons[i], thingz_buttons_ios[i]);
    }
    #endif
    #if MICROPY_THINGZ_TOUCH_BUTTONS_NB
    thingz_button_touch_common_init();
    for(i = 0; i < MICROPY_THINGZ_TOUCH_BUTTONS_NB; i++){
        thingz_button_touch_init(&thingz_touch_buttons[i], thingz_touch_buttons_ios[i]);
    }
    #endif
    #if MICROPY_THINGZ_ACCEL
    thingz_accel_init_gesture();
    for(i = 0; i < TGZ_ACCEL_GESTURE_NONE; i++){
        thingz_accel.gesture_callback[i] = NULL;
    }
    
    common_thingz_accel_init(&thingz_accel, MICROPY_THINGZ_ACCEL_INT_PIN, MICROPY_THINGZ_ACCEL_INT_PIN2, 0);
    #endif
    #if MICROPY_THINGZ_MAGNETO
    common_thingz_compass_init(&thingz_compass, MICROPY_THINGZ_MAGNETO_DRDY, 0);
    #endif
    // return;
    #if MICROPY_THINGZ_LED
    thingz_led_init(&thingz_led, MICROPY_THINGZ_LED_RED, MICROPY_THINGZ_LED_GREEN, MICROPY_THINGZ_LED_BLUE, MICROPY_THINGZ_LED_GND);
    #endif
    #if MICROPY_THINGZ_SOUND
    thingz_sound_init(&thingz_sound, MICROPY_THINGZ_SOUND_LEFT, MICROPY_THINGZ_SOUND_RIGHT, MICROPY_THINGZ_SOUND_GND);
    #endif
    ESP_LOGE(TAG, "thingz radio start heap 8bit %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGE(TAG, "thingz radio start heap 32bit %d", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    ESP_LOGE(TAG, "thingz radio start heap default %d", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    #if MICROPY_THINGZ_RADIO
    thingz_radio_init(&thingz_radio);
    #endif

    #if MICROPY_THINGZ_SCREEN
    thingz_display_init(&thingz_display);
    #endif

    #if MICROPY_THINGZ_LOG
    thingz_log_init(&thingz_log);
    #endif

    ESP_LOGE(TAG, "thingz init end heap 8bit %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGE(TAG, "thingz init end heap 32bit %d", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    ESP_LOGE(TAG, "thingz init end heap default %d", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));

    temperature_sensor_config_t temp_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 50);
    temperature_sensor_install(&temp_config, &temperature_handle);
    temperature_sensor_enable(temperature_handle);

}

void thingz_deinit(void){

    ESP_LOGE(TAG, "thingz deinit start heap 8bit %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGE(TAG, "thingz deinit start heap 32bit %d", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    ESP_LOGE(TAG, "thingz deinit start heap default %d", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    // mp_printf(MP_PYTHON_PRINTER, "thingz deinit 8bit %d\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    // mp_printf(MP_PYTHON_PRINTER, "thingz deinit 32bit %d\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    // mp_printf(MP_PYTHON_PRINTER, "thingz deinit default %d\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
	// thingz_button_touch_common_deinit();
	// thingz_accel_compass_temp_deinit(&thingz_accel, &thingz_compass, &thingz_temp);
	// thingz_radio_deinit(&thingz_radio);
	// thingz_sound_deinit(&thingz_sound);
	// // thingz_microphone_deinit(&thingz_microphone);
	// thingz_display_deinit(&thingz_display);
	// thingz_pwm_global_deinit();
	// microbit_pin_reset();

    #if MICROPY_THINGZ_TOUCH_BUTTONS_NB
    int i;
    for(i = 0; i < MICROPY_THINGZ_TOUCH_BUTTONS_NB; i++){
        thingz_button_touch_deinit(&thingz_touch_buttons[i]);
    }
    thingz_button_touch_common_deinit();
    #endif

    #if MICROPY_THINGZ_ACCEL
    common_thingz_accel_deinit(&thingz_accel);
    #endif

    #if MICROPY_THINGZ_SOUND
    thingz_sound_deinit(&thingz_sound);
    #endif

    #if MICROPY_THINGZ_RADIO
    thingz_radio_deinit(&thingz_radio);
    #endif

    #if MICROPY_THINGZ_SCREEN
    thingz_display_deinit(&thingz_display);
    #endif

    ESP_LOGE(TAG, "thingz deinit end heap 8bit %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGE(TAG, "thingz deinit end heap 32bit %d", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    ESP_LOGE(TAG, "thingz deinit end heap default %d", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    // mp_printf(MP_PYTHON_PRINTER, "thingz deinit end 8bit %d\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    // mp_printf(MP_PYTHON_PRINTER, "thingz deinit end 32bit %d\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    // mp_printf(MP_PYTHON_PRINTER, "thingz deinit end default %d\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    temperature_sensor_disable(temperature_handle);
    temperature_sensor_uninstall(temperature_handle);
}

void thingz_stop_radio(void){
    #if MICROPY_THINGZ_RADIO
    thingz_radio_deinit(&thingz_radio);
    #endif
}