// The "reboot" screen of the firmware's debug menu
// (ports/esp32/debug_mode/reboot/reboot.c), as it is. It disconnects the USB device
// before restarting (mp_usbd_deinit): the simulated board has none, the page talks
// to it directly.
#include "py/mpconfig.h"

void mp_usbd_deinit(void);

#include "ports/esp32/debug_mode/reboot/reboot.c"

void mp_usbd_deinit(void) {
}
