// _thread for the WebAssembly build: MicroPython threads on Emscripten fibers
// (mpthreadport.c), switched where the board's FreeRTOS tasks switch under the GIL.
#ifndef MICROPY_INCLUDED_GALAXIA_MPTHREADPORT_H
#define MICROPY_INCLUDED_GALAXIA_MPTHREADPORT_H

#include <stdint.h>

typedef struct _mp_thread_mutex_t {
    volatile int locked;
} mp_thread_mutex_t;

void mp_thread_init(void);
void mp_thread_gc_others(void);
void mp_thread_deinit(void);

// Waits at most `ms` (it may return earlier, when an input comes): the other threads
// run meanwhile, the Worker sleeps when none can
void galaxia_thread_sleep_ms(int ms);

#endif
