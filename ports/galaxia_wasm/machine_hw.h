// The ESP32-S2's timers, resets, sleep, RTC, watchdog, UART and RMT (machine_hw.c)
#ifndef GALAXIA_MACHINE_HW_H
#define GALAXIA_MACHINE_HW_H

#include <sys/time.h>
#include "idf_shim.h"

// At boot: the reset cause, the RTC clock and memory the page kept
void machine_hw_boot(void);

#endif
