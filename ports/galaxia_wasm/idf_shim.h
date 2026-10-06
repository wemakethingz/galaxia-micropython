// Internal interface of the ESP-IDF shim (idf_shim.c)
#ifndef GALAXIA_IDF_SHIM_H
#define GALAXIA_IDF_SHIM_H

// Runs the esp_timer callbacks that are due, delivers the GPIO edges from the page
// to the interrupt handlers, and sends the screen to the page when it changed.
void idf_shim_poll(void);

// ms until the next esp_timer is due (0 if one is due), -1 if none is armed
int idf_shim_next_timer_ms(void);

#endif
