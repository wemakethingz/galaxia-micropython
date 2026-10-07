// HAL of the galaxia_sim WebAssembly port: time, REPL input/output, polling.

#include <string.h>
#include <sys/time.h>

#include "py/mphal.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "shared/runtime/interrupt_char.h"
#include "idf_shim.h"
#include "mpthreadport.h"

extern int mp_js_ticks_ms(void);
extern double mp_js_time_ms(void);
extern void galaxia_js_stdout(const char *ptr, size_t len);
extern int galaxia_js_stdin_byte(void);
extern void galaxia_js_sleep(int ms);

// The REPL text also goes to the screen, as through the board's dupterm
mp_uint_t thingz_screen_repl_write_from_hal(const char *str, size_t len);

// Give the event loop a turn at most this often while Python runs without sleeping
#define YIELD_PERIOD_MS 20

static uint32_t last_yield;

static void check_interrupt(void) {
    // Ctrl-C typed while a program runs (the REPL reads the other bytes later)
    // is only seen here: the input queue is scanned for it.
    extern int galaxia_js_stdin_take_interrupt(int c);
    if (mp_interrupt_char >= 0 && galaxia_js_stdin_take_interrupt(mp_interrupt_char)) {
        mp_sched_keyboard_interrupt();
    }
}

// The Worker's event loop just had a turn (a real sleep, not a switch to another
// thread)
void galaxia_note_yield(void) {
    last_yield = mp_js_ticks_ms();
}

void galaxia_poll(void) {
    idf_shim_poll();
    uint32_t now = mp_js_ticks_ms();
    if (now - last_yield >= YIELD_PERIOD_MS) {
        last_yield = now;
        galaxia_js_sleep(0);
        idf_shim_poll();
        check_interrupt();
    }
}

int mp_hal_stdin_rx_chr(void) {
    for (;;) {
        int c = galaxia_js_stdin_byte();
        if (c >= 0) {
            return c;
        }
        idf_shim_poll();
        mp_handle_pending(true);
        // Wake for the next timer (screen refresh every 80 ms) or a key; the other
        // threads run meanwhile (the board releases the GIL while it waits for input)
        int wait = idf_shim_next_timer_ms();
        MP_THREAD_GIL_EXIT();
        galaxia_thread_sleep_ms(wait < 0 || wait > 50 ? 50 : wait);
        MP_THREAD_GIL_ENTER();
    }
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len) {
    galaxia_js_stdout(str, len);
    thingz_screen_repl_write_from_hal(str, len);
    return len;
}

void mp_hal_delay_ms(mp_uint_t ms) {
    uint32_t start = mp_hal_ticks_ms();
    for (;;) {
        idf_shim_poll();
        mp_handle_pending(true);
        uint32_t elapsed = mp_hal_ticks_ms() - start;
        if (elapsed >= ms) {
            break;
        }
        int wait = ms - elapsed;
        int next = idf_shim_next_timer_ms();
        if (next >= 0 && next < wait) {
            wait = next;
        }
        MP_THREAD_GIL_EXIT();
        galaxia_thread_sleep_ms(wait);
        MP_THREAD_GIL_ENTER();
        check_interrupt();
    }
}

void mp_hal_delay_us(mp_uint_t us) {
    if (us >= 1000) {
        mp_hal_delay_ms(us / 1000);
        us %= 1000;
    }
    // A short wait spins, as the board's (bit-banged protocols time with it)
    const uint32_t start = mp_hal_ticks_us();
    while (mp_hal_ticks_us() - start < us) {
    }
}

mp_uint_t mp_hal_ticks_ms(void) {
    return mp_js_ticks_ms();
}

// The same clock as esp_timer (fine between the browser clock's ticks)
mp_uint_t mp_hal_ticks_us(void) {
    return (mp_uint_t)esp_timer_get_time();
}

// The CPU's cycle counter (CCOUNT), at the CPU frequency
mp_uint_t mp_hal_ticks_cpu(void) {
    return (mp_uint_t)((uint64_t)mp_hal_ticks_us() * mp_hal_get_cpu_freq() / 1000000);
}

// The RTC clock, as the board's (machine_hw.c): 0 at power-on
uint64_t mp_hal_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

uint64_t mp_hal_time_ns(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000ULL;
}

uintptr_t mp_hal_stdio_poll(uintptr_t poll_flags) {
    extern int galaxia_js_stdin_available(void);
    uintptr_t ret = 0;
    if ((poll_flags & MP_STREAM_POLL_RD) && galaxia_js_stdin_available()) {
        ret |= MP_STREAM_POLL_RD;
    }
    if (poll_flags & MP_STREAM_POLL_WR) {
        ret |= MP_STREAM_POLL_WR;
    }
    return ret;
}
