// Board services the Thingz modules call, for the WebAssembly build: settings
// (thingz_memory, over the in-memory NVS of idf_shim.c) and the last exception
// shown by the screen header (debug_mode). The debug menu itself is not built.

#include <string.h>

#include "py/runtime.h"
#include "py/mpprint.h"
#include "common-thingz/thingz_memory/thingz_memory.h"
#include "common-thingz/thingz_screen/thingz_screen.h"
#include "debug_mode/debug_mode.h"

// ---- thingz_memory ----

static nvs_handle nvs_main = 1;

void thingz_memory_init(void) {
}

nvs_handle *thingz_memory_get_handle(void) {
    return &nvs_main;
}

const char *thingz_memory_get_pcb_version_name(void) {
    return "web";
}

int32_t thingz_memory_get_setting(const char *name, void *value) {
    return nvs_get_u32(nvs_main, name, (uint32_t *)value);
}

int32_t thingz_memory_set_setting(const char *name, void *value) {
    return nvs_set_u32(nvs_main, name, *(uint32_t *)value);
}

uint8_t thingz_memory_is_eeprom_corrupt(void) {
    return 0;
}

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
