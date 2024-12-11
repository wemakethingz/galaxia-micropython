#ifndef COMMON_THINGZ_I2C_H_
#define COMMON_THINGZ_I2C_H_

#include "stdint.h"
#include "driver/gpio.h"

void common_thingz_i2c_init(int bus, int8_t scl, int8_t sda, uint32_t freq, bool enable_pullups);

int32_t common_thingz_i2c_write(uint8_t addr, uint8_t bus_id, uint8_t reg, uint8_t *bufp, uint16_t len);

int32_t common_thingz_i2c_read(uint8_t addr, uint8_t bus_id, uint8_t reg, uint8_t *bufp, uint16_t len);

#endif