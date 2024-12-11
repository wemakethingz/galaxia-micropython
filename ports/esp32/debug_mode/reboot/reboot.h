#ifndef MICROPY_INCLUDED_ESP32S2_DEBUG_MODE_REBOOT_H
#define MICROPY_INCLUDED_ESP32S2_DEBUG_MODE_REBOOT_H

void debug_mode_show_reboot_screen(void);

void debug_mode_reboot_enter(void);
void debug_mode_reboot_exit(void);

#endif