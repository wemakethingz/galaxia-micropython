// MicroPython configuration of the galaxia_sim WebAssembly port: the webassembly
// port's (MicroPython 1.24), without the JS proxies, with a blocking REPL as on
// the board and the Galaxia board configuration.

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>      // as the esp32 port's: esp_system.h -> esp_err.h includes it

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
// As on the esp32 port: microcontroller.set_file_to_execute() stops the program with a
// ReloadInterrupt and the board runs the new file
#define MICROPY_RELOAD_EXCEPTION    (1)
#define MICROPY_REPL_INFO           (1)
#define MICROPY_HELPER_REPL         (1)
#define MICROPY_REPL_AUTO_INDENT    (1)
#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_ENABLE_DOC_STRING   (0)
#define MICROPY_WARNINGS            (1)
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_FLOAT)
// errno of the board's C library (newlib): ETIMEDOUT is 116 (newlib/errno.h)
#define MICROPY_USE_INTERNAL_ERRNO  (0)
#define MICROPY_USE_INTERNAL_PRINTF (0)
// Time as on the board: the esp32 port's time module over the chip's RTC clock
// (gettimeofday), which starts at 0 at power-on; MicroPython's epoch is 2000
#define gettimeofday(tv, tz) galaxia_gettimeofday(tv, tz)
#define settimeofday(tv, tz) galaxia_settimeofday(tv, tz)
struct timeval;
struct timezone;
int galaxia_gettimeofday(struct timeval *tv, void *tz);
int galaxia_settimeofday(const struct timeval *tv, const struct timezone *tz);
#define MICROPY_PY_RANDOM_SEED_INIT_FUNC (mp_js_random_u32())
#define MICROPY_PY_TIME_GMTIME_LOCALTIME_MKTIME (1)
#define MICROPY_PY_TIME_TIME_TIME_NS (1)
#define MICROPY_PY_TIME_INCLUDEFILE "ports/esp32/modtime.c"
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

// machine: the esp32 port's (ports/esp32/mpconfigport.h), over the simulated chip
// (machine_hw.c). Not here: I2S, SD card (not on the Galaxia).
#define MICROPY_PY_MACHINE                  (1)
#define MICROPY_PY_MACHINE_INCLUDEFILE      "ports/esp32/modmachine.c"
#define MICROPY_PY_MACHINE_RESET            (1)
#define MICROPY_PY_MACHINE_BARE_METAL_FUNCS (1)
#define MICROPY_PY_MACHINE_DISABLE_IRQ_ENABLE_IRQ (1)
#define MICROPY_PY_MACHINE_ADC              (1)
#define MICROPY_PY_MACHINE_ADC_INCLUDEFILE  "ports/esp32/machine_adc.c"
#define MICROPY_PY_MACHINE_ADC_ATTEN_WIDTH  (1)
#define MICROPY_PY_MACHINE_ADC_INIT         (1)
#define MICROPY_PY_MACHINE_ADC_READ         (1)
#define MICROPY_PY_MACHINE_ADC_READ_UV      (1)
#define MICROPY_PY_MACHINE_ADC_BLOCK        (1)
#define MICROPY_PY_MACHINE_ADC_BLOCK_INCLUDEFILE "ports/esp32/machine_adc_block.c"
#define MICROPY_PY_MACHINE_PIN_MAKE_NEW     mp_pin_make_new
#define MICROPY_PY_MACHINE_BITSTREAM        (1)
#define MICROPY_PY_MACHINE_DHT_READINTO     (1)
#define MICROPY_PY_MACHINE_PULSE            (1)
#define MICROPY_PY_MACHINE_PWM              (1)
#define MICROPY_PY_MACHINE_PWM_DUTY         (1)
#define MICROPY_PY_MACHINE_PWM_INCLUDEFILE  "ports/esp32/machine_pwm.c"
#define MICROPY_PY_MACHINE_I2C              (1)
#define MICROPY_PY_MACHINE_I2C_TRANSFER_WRITE1 (1)
#define MICROPY_PY_MACHINE_SOFTI2C          (1)
#define MICROPY_PY_MACHINE_SPI              (1)
#define MICROPY_PY_MACHINE_SOFTSPI          (1)
#define MICROPY_PY_MACHINE_DAC              (1)
#define MICROPY_PY_MACHINE_I2S              (0)
#define MICROPY_PY_MACHINE_UART             (1)
#define MICROPY_PY_MACHINE_UART_INCLUDEFILE "ports/esp32/machine_uart.c"
#define MICROPY_PY_MACHINE_UART_SENDBREAK   (1)
#define MICROPY_PY_MACHINE_UART_IRQ         (1)
#define MICROPY_PY_MACHINE_WDT              (1)
#define MICROPY_PY_MACHINE_WDT_INCLUDEFILE  "ports/esp32/machine_wdt.c"
#define MICROPY_PY_ONEWIRE                  (1)
#define MICROPY_HW_ENABLE_SDCARD            (0)
#define MICROPY_HW_SOFTSPI_MIN_DELAY        (0)
#define MICROPY_HW_SOFTSPI_MAX_BAUDRATE     (esp_rom_get_cpu_ticks_per_us() * 1000000 / 200)
// machine.bootloader(): the RTC register that makes the ROM wait for a download
#define MICROPY_ESP32_USE_BOOTLOADER_RTC    (1)
#define MICROPY_BOARD_ENTER_BOOTLOADER(nargs, args) machine_bootloader_rtc()
#define MICROPY_PY_MACHINE_BOOTLOADER       (1)
#define MICROPY_HW_RTC_USER_MEM_MAX         (2048)

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
