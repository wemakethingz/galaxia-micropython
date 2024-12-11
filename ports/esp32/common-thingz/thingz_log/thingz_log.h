#ifndef COMMON_THINGZ_LOG_H_
#define COMMON_THINGZ_LOG_H_

#include "nvs_flash.h"
#include "nvs.h"

#include "py/obj.h"
#include "py/objlist.h"

extern const mp_obj_type_t thingz_log_type;

typedef struct {
	mp_obj_base_t base;
    uint32_t readPos;
    uint32_t writePos;
    char** columns;
    uint8_t columns_len;
} thingz_log_obj_t;

void thingz_log_init(thingz_log_obj_t* log);
void thingz_log_deinit(thingz_log_obj_t* log);

#endif