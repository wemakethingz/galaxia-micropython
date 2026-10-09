// HAL of the galaxia_sim WebAssembly port
// (Same guard as ports/esp32/mphalport.h: a firmware file that includes that one by
// its quoted name, as modmicrocontroller.c, gets it alone.)
#ifndef INCLUDED_MPHALPORT_H
#define INCLUDED_MPHALPORT_H

#include "py/obj.h"
#include "shared/runtime/interrupt_char.h"

int mp_hal_stdin_rx_chr(void);
mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len);

void mp_hal_delay_ms(mp_uint_t ms);
void mp_hal_delay_us(mp_uint_t us);
mp_uint_t mp_hal_ticks_ms(void);
mp_uint_t mp_hal_ticks_us(void);
mp_uint_t mp_hal_ticks_cpu(void);
uint64_t mp_hal_time_ms(void);
uint64_t mp_hal_time_ns(void);
uintptr_t mp_hal_stdio_poll(uintptr_t poll_flags);

int mp_hal_get_interrupt_char(void);

// Input the firmware itself queues (microcontroller.set_file_to_execute: Ctrl-C,
// Ctrl-D), read before the page's
#include "py/ringbuf.h"
extern ringbuf_t stdin_ringbuf;

// Runs the due timers and, at most every few ms, lets the Worker's event loop run.
void galaxia_poll(void);

// As the esp32 port (ports/esp32/mphalport.h): what machine and the bit-banged
// buses (SoftI2C, SoftSPI, onewire, DHT) need
#define mp_hal_delay_us_fast(us) mp_hal_delay_us(us)
uint32_t mp_hal_get_cpu_freq(void);
mp_uint_t mp_begin_atomic_section(void);
void mp_end_atomic_section(mp_uint_t state);
#define MICROPY_BEGIN_ATOMIC_SECTION() mp_begin_atomic_section()
#define MICROPY_END_ATOMIC_SECTION(state) mp_end_atomic_section(state)
#define mp_hal_quiet_timing_enter() MICROPY_BEGIN_ATOMIC_SECTION()
#define mp_hal_quiet_timing_exit(irq_state) MICROPY_END_ATOMIC_SECTION(irq_state)
void mp_hal_wake_main_task(void);
void mp_hal_wake_main_task_from_isr(void);

// C-level pin HAL
#include "driver/gpio.h"
#define MP_HAL_PIN_FMT "%u"
#define mp_hal_pin_obj_t gpio_num_t
mp_hal_pin_obj_t machine_pin_get_id(mp_obj_t pin_in);
#define mp_hal_get_pin_obj(o) machine_pin_get_id(o)
#define mp_hal_pin_name(p) (p)
static inline void mp_hal_pin_input(mp_hal_pin_obj_t pin) {
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}
static inline void mp_hal_pin_output(mp_hal_pin_obj_t pin) {
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT);
}
static inline void mp_hal_pin_open_drain(mp_hal_pin_obj_t pin) {
    esp_rom_gpio_pad_select_gpio(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
}
static inline void mp_hal_pin_od_low(mp_hal_pin_obj_t pin) {
    gpio_set_level(pin, 0);
}
static inline void mp_hal_pin_od_high(mp_hal_pin_obj_t pin) {
    gpio_set_level(pin, 1);
}
static inline int mp_hal_pin_read(mp_hal_pin_obj_t pin) {
    return gpio_get_level(pin);
}
static inline void mp_hal_pin_write(mp_hal_pin_obj_t pin, int v) {
    gpio_set_level(pin, v);
}

spi_host_device_t machine_hw_spi_get_host(mp_obj_t in);

// An ESP-IDF error as an OSError, as the esp32 port
#define check_esp_err(code) check_esp_err_(code)
void check_esp_err_(esp_err_t code);

#include <errno.h>

// PEP 475: retry syscalls on EINTR (VFS posix)
#define MP_HAL_RETRY_SYSCALL(ret, syscall, raise) \
    { \
        for (;;) { \
            ret = syscall; \
            if (ret == -1) { \
                int err = errno; \
                if (err == EINTR) { \
                    mp_handle_pending(true); \
                    continue; \
                } \
                raise; \
            } \
            break; \
        } \
    }

#endif // INCLUDED_MPHALPORT_H
