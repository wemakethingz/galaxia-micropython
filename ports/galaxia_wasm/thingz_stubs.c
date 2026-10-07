// Board services the Thingz modules call, for the WebAssembly build: the last
// exception shown by the screen header (debug_mode). The debug menu itself is not
// built. (thingz_memory is the firmware's, over the EEPROM of peripherals.c.)

#include <string.h>

#include "py/runtime.h"
#include "py/mpprint.h"
#include "common-thingz/thingz_memory/thingz_memory.h"
#include "common-thingz/thingz_screen/thingz_screen.h"
#include "debug_mode/debug_mode.h"

// ---- debug_mode: the last exception (same logic as debug_mode/debug_mode.c) ----

static char debug_last_exception[512];
static uint8_t debug_expose_exception;

static void thgz_debug_exception_printer(void *env, const char *str, size_t len) {
    (void)env;
    size_t length = strlen(debug_last_exception);
    if (length < sizeof(debug_last_exception) - 1) {
        size_t n = len < sizeof(debug_last_exception) - 1 - length ? len : sizeof(debug_last_exception) - 1 - length;
        memcpy(debug_last_exception + length, str, n);
    }
}

const mp_print_t thgz_debug_exception_print = {
    NULL,
    thgz_debug_exception_printer
};

void debug_mode_expose_last_exception(void) {
    debug_expose_exception = 1;
}

char *debug_mode_get_last_exception(void) {
    return debug_expose_exception ? debug_last_exception : NULL;
}

void debug_mode_reset_last_exception(void) {
    memset(debug_last_exception, 0, sizeof(debug_last_exception));
    debug_expose_exception = 0;
}

void debug_mode_start(void) {
}

void debug_mode_stop(void) {
}

void debug_mode_need_exit(void) {
}

// ---- REPL text to the screen, as the board's dupterm does ----

static bool screen_ready;

void galaxia_screen_ready(void) {
    screen_ready = true;
}

mp_uint_t thingz_screen_repl_write_from_hal(const char *str, size_t len) {
    if (!screen_ready) {
        return len;
    }
    int errcode;
    return thingz_screen_repl_write(&thingz_screen.repl, str, len, &errcode);
}
