// MicroPython configuration of the galaxia_sim WebAssembly port: the webassembly
// port's (MicroPython 1.24), without the JS proxies, with a blocking REPL as on
// the board and the Galaxia board configuration.

#include <stdint.h>
#include <stdlib.h>

#include "mpconfigboard.h"

#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES)

#define MICROPY_ALLOC_PATH_MAX      (256)
#define MICROPY_PERSISTENT_CODE_LOAD (1)
#define MICROPY_READER_VFS          (MICROPY_VFS)
#define MICROPY_ENABLE_GC           (1)
// As on the esp32 port: Python frames on the C stack, recursion depth checked
#define MICROPY_ENABLE_PYSTACK      (0)
#define MICROPY_STACK_CHECK         (1)
#define MICROPY_KBD_EXCEPTION       (1)
#define MICROPY_HELPER_REPL         (1)
#define MICROPY_REPL_AUTO_INDENT    (1)
#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_ENABLE_DOC_STRING   (0)
#define MICROPY_WARNINGS            (1)
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_FLOAT)
// errno of the board's C library (newlib): ETIMEDOUT is 116 (newlib/errno.h)
#define MICROPY_USE_INTERNAL_ERRNO  (0)
#define MICROPY_USE_INTERNAL_PRINTF (0)
#define MICROPY_EPOCH_IS_1970       (1)
#define MICROPY_PY_RANDOM_SEED_INIT_FUNC (mp_js_random_u32())
#define MICROPY_PY_TIME_GMTIME_LOCALTIME_MKTIME (1)
#define MICROPY_PY_TIME_TIME_TIME_NS (1)
#define MICROPY_PY_TIME_INCLUDEFILE "ports/webassembly/modtime.c"
#define MICROPY_VFS                 (1)
#define MICROPY_FATFS_ENABLE_LFN    (1)
#define MICROPY_FATFS_RPATH         (2)
#define MICROPY_FATFS_MAX_SS        (4096)
#define MICROPY_FATFS_LFN_CODE_PAGE 437
#define MICROPY_FATFS_USE_LABEL     (1)
#define MICROPY_PY_SYS_PLATFORM     "galaxia"
#define MICROPY_PY_SYS_STDFILES     (1)
#define MICROPY_PY_OS_DUPTERM       (0)
// _thread as on the board (esp32 port: GIL, released every 32 jumps), on fibers
// (mpthreadport.c)
#define MICROPY_PY_THREAD           (1)
#define MICROPY_PY_THREAD_GIL       (1)
#define MICROPY_PY_THREAD_GIL_VM_DIVISOR (32)
#define MICROPY_STACK_CHECK_MARGIN  (1024)
#define MICROPY_PY_ASYNCIO          (1)

// Network: the esp32 port's network (WLAN) and socket modules, over the simulated
// Wi-Fi chip and lwIP of net.c
#define MICROPY_PY_NETWORK          (1)
#define MICROPY_PY_NETWORK_HOSTNAME_DEFAULT "GALAXIA"
#define MICROPY_PY_NETWORK_INCLUDEFILE "ports/esp32/modnetwork.h"
#define MICROPY_PY_NETWORK_MODULE_GLOBALS_INCLUDEFILE "ports/esp32/modnetwork_globals.h"
#define MICROPY_PY_NETWORK_WLAN     (1)
#define MICROPY_HW_ENABLE_MDNS_QUERIES   (1)
#define MICROPY_HW_ENABLE_MDNS_RESPONDER (1)
// As on the board (MICROPY_PY_WEBREPL): socket callbacks run from the poll hook
#define MICROPY_PY_SOCKET_EVENTS    (1)

// Every few bytecodes: run the due esp_timer callbacks (screen refresh, button
// debounce), and now and then give the Worker's event loop a turn so that
// messages from the page (keys, buttons) get in even during a busy loop.
#define MICROPY_VM_HOOK_COUNT (1000)     // every ~0.1 ms: plenty for 50/80 ms timers
#define MICROPY_VM_HOOK_INIT static unsigned int vm_hook_divisor = MICROPY_VM_HOOK_COUNT;
#define MICROPY_VM_HOOK_POLL if (--vm_hook_divisor == 0) { \
        vm_hook_divisor = MICROPY_VM_HOOK_COUNT; \
        extern void galaxia_poll(void); \
        galaxia_poll(); \
}
#define MICROPY_VM_HOOK_LOOP MICROPY_VM_HOOK_POLL
#define MICROPY_VM_HOOK_RETURN MICROPY_VM_HOOK_POLL

#define MICROPY_EVENT_POLL_HOOK \
    do { \
        extern void galaxia_poll(void); \
        extern void mp_handle_pending(bool); \
        extern void socket_events_handler(void); \
        galaxia_poll(); \
        socket_events_handler(); \
        mp_handle_pending(true); \
    } while (0);

#define MP_SSIZE_MAX (0x7fffffff)
#define UINT_FMT "%u"
#define INT_FMT "%d"

typedef int mp_int_t; // must be pointer size
typedef unsigned mp_uint_t; // must be pointer size
typedef long mp_off_t;

#define MP_STATE_PORT MP_STATE_VM

#define _GNU_SOURCE

uint32_t mp_js_random_u32(void);
