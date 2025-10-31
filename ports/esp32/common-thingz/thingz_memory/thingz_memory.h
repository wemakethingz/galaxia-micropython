#ifndef COMMON_THINGZ_MEMORY_H_
#define COMMON_THINGZ_MEMORY_H_

#include "nvs_flash.h"
#include "nvs.h"

#define THINGZ_VERSION_1_0_5 255
#define THINGZ_VERSION_1_0_6 0
#define THINGZ_VERSION_1_0_7 1
#define THINGZ_VERSION_1_0_8 2
#define THINGZ_VERSION_1_0_9 3

#define THINGZ_MEMORY_EEPROM_CORRUPT 1
#define THINGZ_MEMORY_EEPROM_UNKNOWN_VERSION 2

void thingz_memory_init(void);
nvs_handle* thingz_memory_get_handle(void);

int32_t thingz_memory_read_eeprom(uint16_t addr, uint8_t* value);
int32_t thingz_memory_write_eeprom(uint16_t addr, uint8_t value);

int32_t thingz_memory_write_array_eeprom(uint16_t addr, uint8_t* value, uint8_t len);
int32_t thingz_memory_read_array_eeprom(uint16_t addr, uint8_t* value, uint8_t len);

int32_t thingz_memory_get_setting(const char* name, void* value);
int32_t thingz_memory_set_setting(const char* name, void* value);

const char* thingz_memory_get_pcb_version_name(void);

void thingz_memory_replicate_settings(void);
void thingz_memory_load_eeprom_info(void);
uint8_t thingz_memory_is_eeprom_corrupt(void);

#endif