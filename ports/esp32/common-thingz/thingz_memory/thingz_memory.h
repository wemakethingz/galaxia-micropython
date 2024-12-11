#ifndef COMMON_THINGZ_MEMORY_H_
#define COMMON_THINGZ_MEMORY_H_

#include "nvs_flash.h"
#include "nvs.h"

void thingz_memory_init(void);
nvs_handle* thingz_memory_get_handle(void);
#endif