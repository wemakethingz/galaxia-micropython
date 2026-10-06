// Shadows ports/esp32/modnetwork.h in the WebAssembly build: the radio module
// only needs to start Wi-Fi (simulated by peripherals.c).
#ifndef GALAXIA_MODNETWORK_H
#define GALAXIA_MODNETWORK_H
void esp_initialise_wifi(void);
#endif
