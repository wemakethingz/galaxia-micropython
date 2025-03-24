#ifndef COMMON_THINGZ_H_
#define COMMON_THINGZ_H_

#include "thingz_button/thingz_button.h"
#include "thingz_button_touch/thingz_button_touch.h"
#include "thingz_screen/thingz_screen.h"
#include "thingz_accel/thingz_accel.h"
#include "thingz_led/thingz_led.h"
#include "thingz_sound/thingz_sound.h"
#include "thingz_radio/thingz_radio.h"
#include "thingz_display/thingz_display.h"
#include "thingz_log/thingz_log.h"

#define THINGZ_INPUT_TARGET_PYTHON 0
#define THINGZ_INPUT_TARGET_TGZ_DEBUG 1

#define THINGZ_INPUT_EVENT_BUTTON_PRESS 0
#define THINGZ_INPUT_EVENT_BUTTON_RELEASE 1

#if MICROPY_THINGZ_BUTTONS_NB
    extern thingz_button_obj_t thingz_buttons[MICROPY_THINGZ_BUTTONS_NB];
#endif

#if MICROPY_THINGZ_TOUCH_BUTTONS_NB
    extern thingz_button_touch_obj_t thingz_touch_buttons[MICROPY_THINGZ_TOUCH_BUTTONS_NB];
#endif


#if MICROPY_THINGZ_ACCEL
    extern thingz_accel_obj_t thingz_accel;
#endif

#if MICROPY_THINGZ_MAGNETO
    extern thingz_compass_obj_t thingz_compass;
#endif

#if MICROPY_THINGZ_LED
    extern thingz_led_obj_t thingz_led;
#endif
#if MICROPY_THINGZ_SOUND
    extern thingz_sound_obj_t thingz_sound;
#endif
#if MICROPY_THINGZ_RADIO
    extern thingz_radio_obj_t thingz_radio;
#endif
#if MICROPY_THINGZ_SCREEN
    extern thingz_display_obj_t thingz_display;
#endif

#if MICROPY_THINGZ_LOG
    extern thingz_log_obj_t thingz_log;
#endif

typedef struct{
    void* obj;
    uint8_t event;
} thingz_input_event_t;

typedef void (* input_event_callback)(thingz_input_event_t);

uint8_t thingz_get_input_target(void);
void thingz_set_input_target(uint8_t target, void (*callback)(thingz_input_event_t));
input_event_callback thingz_get_input_event_callback(void);

void thingz_set_python_file_to_exec(const char* name);
char* thingz_get_python_file_to_exec(uint8_t cp437, uint8_t print);

void thingz_print_filename(const char* name);

void thingz_init(void);
void thingz_deinit(void);

void thingz_stop_radio(void);

#endif