#ifndef MICROPY_INCLUDED_ESP32S2_DEBUG_MODE_H
#define MICROPY_INCLUDED_ESP32S2_DEBUG_MODE_H

#include "py/mpprint.h"
#include <py/obj.h>

#define DEBUG_SCREEN_HOME 0
#define DEBUG_SCREEN_SENSOR_BUTTONS 1
#define DEBUG_SCREEN_SENSOR_LED 2
#define DEBUG_SCREEN_SENSORS 3
#define DEBUG_SCREEN_LOCAL_VAR 4
#define DEBUG_SCREEN_LAST_ERROR 5
#define DEBUG_SCREEN_CONFIG 6
#define DEBUG_SCREEN_REBOOT 7
#define DEBUG_SCREEN_MAX 8

#define DEBUG_SCREEN_WIDTH_IN_TILES 27
#define DEBUG_SCREEN_HEIGHT_IN_TILES 9

extern const mp_print_t thgz_debug_exception_print;

void debug_mode_clear_screen(void);
void debug_mode_print_str(uint8_t x, uint8_t y, const char* str, uint32_t background, uint32_t frontground);
void debug_mode_set_current_screen(uint8_t screen);
void debug_mode_set_header_text(char* title);
size_t debug_mode_utf8_strlen(const byte *b);

void debug_mode_print_header(void);

char* debug_mode_get_last_exception(void);
void debug_mode_reset_last_exception(void);
void debug_mode_start(void);
void debug_mode_stop(void);
void debug_mode_need_exit(void);

#endif