// Internal interface of the ESP-IDF shim (idf_shim.c)
#ifndef GALAXIA_IDF_SHIM_H
#define GALAXIA_IDF_SHIM_H

// Runs the esp_timer callbacks that are due, delivers the GPIO edges from the page
// to the interrupt handlers, and sends the screen to the page when it changed.
void idf_shim_poll(void);

// ms until the next esp_timer is due (0 if one is due), -1 if none is armed
int idf_shim_next_timer_ms(void);

// GPIO pads, for the peripherals: initial state, output register (-1 if the pad
// is not a GPIO output), routed signal, pull (true and its level if pulled)
void gpio_shim_init(void);
int gpio_shim_output(int pin);
int gpio_shim_signal(int pin);
bool gpio_shim_pulled(int pin, int *level);

#endif
