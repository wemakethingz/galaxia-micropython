// Peripherals of the WebAssembly Galaxia (see peripherals.c)
#ifndef GALAXIA_PERIPHERALS_H
#define GALAXIA_PERIPHERALS_H

#include "idf/idf_all.h"

// Delivers the inputs the page changed (touch pads, radio packets) and sends the
// outputs that changed (LED); called from idf_shim_poll()
void peripherals_poll(void);

// GPIO direction change, for the LED light measurement
void peripherals_gpio_direction(gpio_num_t pin, gpio_mode_t mode);

// The level a DAC channel holds on its pad (0-255), -1 if it does not drive it
int dac_pad_level(int pin);

#endif
