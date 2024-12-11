#ifndef THINGZ_MICROPHONE_H_
#define THINGZ_MICROPHONE_H_

#include "py/obj.h"

#include "pins.h"

#define THINGZ_MICROPHONE_EVENT_NONE 0
#define THINGZ_MICROPHONE_EVENT_LOUD 1
#define THINGZ_MICROPHONE_EVENT_QUIET 2

extern const mp_obj_type_t thingz_microphone_type;

typedef struct {
	mp_obj_base_t base;
    const mcu_pin_obj_t* pinJackMIC;
    
    uint8_t sound_event_loud_level;
    uint8_t sound_event_quiet_level;
    uint8_t sound_event_history_index;
    uint8_t sound_event_history_length;
    uint8_t* sound_event_history;
} thingz_microphone_obj_t;

extern const mp_obj_type_t thingz_sound_event_type;

typedef struct {
	mp_obj_base_t base;
   
} thingz_sound_event_obj_t;

extern const thingz_sound_event_obj_t thingz_sound_event_loud_obj;
extern const thingz_sound_event_obj_t thingz_sound_event_quiet_obj;
extern const thingz_sound_event_obj_t thingz_sound_event_none_obj;

void thingz_microphone_init(thingz_microphone_obj_t* microphone, const mcu_pin_obj_t* pinJackMIC);
void thingz_microphone_deinit(thingz_microphone_obj_t* microphone);
#endif