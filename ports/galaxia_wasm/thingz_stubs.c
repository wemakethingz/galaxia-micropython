// Board services the Thingz modules call, for the WebAssembly build. (The debug
// menu, debug_mode, and thingz_memory are the firmware's.)

#include <string.h>

#include "py/runtime.h"
#include "py/mpprint.h"
#include "common-thingz/thingz_memory/thingz_memory.h"
#include "common-thingz/thingz_screen/thingz_screen.h"
#include "debug_mode/debug_mode.h"

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
