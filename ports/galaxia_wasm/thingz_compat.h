// Force-included in the Thingz sources of the WebAssembly build: what they use of
// the esp32 port that this single-threaded build does not have.
#ifndef GALAXIA_THINGZ_COMPAT_H
#define GALAXIA_THINGZ_COMPAT_H

#include "py/mpstate.h"

#if !MICROPY_PY_THREAD
// thingz_screen.c runs the RAW refresh "in MicroPython context" from the timer task;
// here timers already run on the MicroPython thread.
static inline mp_state_thread_t *mp_thread_get_state(void) {
    return &mp_state_ctx.thread;
}
#define mp_thread_init_state(ts, stack_size, locals, globals) ((void)(ts))
#define mp_thread_set_state(ts) ((void)(ts))
#endif

#endif
