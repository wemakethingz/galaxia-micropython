#ifndef COMMON_THINGZ_RADIO_H_
#define COMMON_THINGZ_RADIO_H_

#include "py/obj.h"

extern const mp_obj_type_t thingz_radio_type;

typedef struct {
	mp_obj_base_t base;
    uint8_t channel;
    bool enabled;
} thingz_radio_obj_t;


void thingz_radio_init(thingz_radio_obj_t* radio);
void thingz_radio_deinit(thingz_radio_obj_t* radio);

#endif