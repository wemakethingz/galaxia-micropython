// The parts of the ESP32-S2 below ports/esp32's machine module, for the WebAssembly
// build (the GPIO pads, the LEDC, I2C, ADC, DAC and touch are in idf_shim.c and
// peripherals.c):
//   - the 4 general purpose timers (2 groups of 2, APB clock, 64-bit counters, alarm,
//     auto-reload, interrupt), as timer_ll registers;
//   - resets and their cause, deep and light sleep, the RTC clock (0 at power-on) and
//     the RTC memory, both kept across the resets that are not a power-on;
//   - the task watchdog (a missed feed resets the board, cause TASK_WDT);
//   - the 2 UARTs (nothing is wired to their pins: what is sent leaves at the baud
//     rate, nothing comes in);
//   - the RMT (bitstream: NeoPixels), the CPU frequency, the eFuse MAC address.

#include <string.h>
#include <sys/time.h>
#include <emscripten.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "py/objstr.h"
#include "idf/idf_all.h"
#include "mpthreadport.h"
#include "machine_hw.h"

extern void galaxia_js_mac(uint8_t *mac);
extern void galaxia_js_sleep(int ms);
extern int galaxia_js_gpio_get(int pin);
extern int galaxia_js_touch_get(int pad);

#define NOW() esp_timer_get_time()

// ====================================================================== errors

// As ports/esp32/mphalport.c: an ESP-IDF error as an OSError (its name as message)
void check_esp_err_(esp_err_t code) {
    if (code != ESP_OK) {
        uint32_t pcode = -code;
        switch (code) {
            case ESP_ERR_NO_MEM:
                pcode = MP_ENOMEM;
                break;
            case ESP_ERR_TIMEOUT:
                pcode = MP_ETIMEDOUT;
                break;
            case ESP_ERR_NOT_SUPPORTED:
                pcode = MP_EOPNOTSUPP;
                break;
        }
        mp_obj_str_t *o_str = m_new_obj_maybe(mp_obj_str_t);
        if (o_str == NULL) {
            mp_raise_OSError(pcode);
            return;
        }
        o_str->base.type = &mp_type_str;
        o_str->data = (const byte *)esp_err_to_name(code);
        o_str->len = strlen((char *)o_str->data);
        o_str->hash = qstr_compute_hash(o_str->data, o_str->len);
        mp_obj_t args[2] = { MP_OBJ_NEW_SMALL_INT(pcode), MP_OBJ_FROM_PTR(o_str)};
        nlr_raise(mp_obj_exception_make_new(&mp_type_OSError, 2, 0, args));
    }
}

// ========================================================== CPU, atomic sections

static uint32_t cpu_mhz = 160;      // CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ

uint32_t esp_rom_get_cpu_ticks_per_us(void) {
    return cpu_mhz;
}

uint32_t mp_hal_get_cpu_freq(void) {
    return cpu_mhz * 1000000;
}

esp_err_t esp_pm_configure(const void *config) {
    const esp_pm_config_t *pm = config;
    cpu_mhz = pm->max_freq_mhz;
    return ESP_OK;
}

void esp_rom_delay_us(uint32_t us) {
    mp_hal_delay_us(us);
}

// One thread switched cooperatively: nothing interrupts a section
mp_uint_t mp_begin_atomic_section(void) {
    return 0;
}

void mp_end_atomic_section(mp_uint_t state) {
}

// The board wakes its main task from an interrupt; here the main loop polls
void mp_hal_wake_main_task(void) {
}

void mp_hal_wake_main_task_from_isr(void) {
}

esp_err_t esp_efuse_mac_get_default(uint8_t *mac) {
    galaxia_js_mac(mac);
    return ESP_OK;
}

// ================================================= boot, resets, RTC clock and memory

// Kept by machine_rtc.c in RTC memory (RTC_NOINIT_ATTR): survive the resets
extern uint32_t rtc_user_mem_magic;
extern uint16_t rtc_user_mem_len;
extern uint8_t rtc_user_mem_data[MICROPY_HW_RTC_USER_MEM_MAX];

static esp_reset_reason_t boot_reason = ESP_RST_POWERON;
static esp_sleep_wakeup_cause_t wake_cause = ESP_SLEEP_WAKEUP_UNDEFINED;
static int64_t rtc_offset_us;       // RTC time = rtc_offset_us + esp_timer_get_time()

// The RTC memory as the page keeps it across a reset: magic, length, data
EMSCRIPTEN_KEEPALIVE
uint8_t *galaxia_rtc_mem(void) {
    static uint8_t image[4 + 2 + MICROPY_HW_RTC_USER_MEM_MAX];
    memcpy(image, &rtc_user_mem_magic, 4);
    memcpy(image + 4, &rtc_user_mem_len, 2);
    memcpy(image + 6, rtc_user_mem_data, MICROPY_HW_RTC_USER_MEM_MAX);
    return image;
}

EMSCRIPTEN_KEEPALIVE
int galaxia_rtc_mem_size(void) {
    return 4 + 2 + MICROPY_HW_RTC_USER_MEM_MAX;
}

EMSCRIPTEN_KEEPALIVE
double galaxia_rtc_now_us(void) {
    return (double)(rtc_offset_us + NOW());
}

// At boot: what the page kept from the previous run (Module.galaxia.boot)
void machine_hw_boot(void) {
    boot_reason = EM_ASM_INT({ return (Module.galaxia.boot && Module.galaxia.boot.reason) || 1; });
    wake_cause = EM_ASM_INT({ return (Module.galaxia.boot && Module.galaxia.boot.wake) || 0; });
    rtc_offset_us = (int64_t)EM_ASM_DOUBLE({ return (Module.galaxia.boot && Module.galaxia.boot.rtcUs) || 0; }) - NOW();
    if (boot_reason == ESP_RST_POWERON) {
        rtc_offset_us = -NOW();
        return;
    }
    // The RTC memory survives
    uint8_t *image = galaxia_rtc_mem();
    if (EM_ASM_INT({
        const m = Module.galaxia.boot && Module.galaxia.boot.rtcMem;
        if (!m) return 0;
        HEAPU8.set(m.subarray(0, $1), $0);
        return 1;
    }, image, galaxia_rtc_mem_size())) {
        memcpy(&rtc_user_mem_magic, image, 4);
        memcpy(&rtc_user_mem_len, image + 4, 2);
        memcpy(rtc_user_mem_data, image + 6, MICROPY_HW_RTC_USER_MEM_MAX);
    }
}

esp_reset_reason_t esp_reset_reason(void) {
    return boot_reason;
}

// The board resets: the page starts a new run with the cause, the RTC clock and
// memory (and, for a deep sleep, after `sleep_ms`, -1: until reset)
NORETURN static void board_reset(esp_reset_reason_t reason, esp_sleep_wakeup_cause_t wake, int64_t sleep_ms) {
    EM_ASM({ Module.galaxia.reset($0, $1, $2); }, reason, wake, (double)sleep_ms);
    for (;;) {
        galaxia_js_sleep(1000);     // the Worker is closed
    }
}

void esp_restart(void) {
    board_reset(ESP_RST_SW, ESP_SLEEP_WAKEUP_UNDEFINED, 0);
}

int galaxia_gettimeofday(struct timeval *tv, void *tz) {
    const int64_t us = rtc_offset_us + NOW();
    tv->tv_sec = us / 1000000;
    tv->tv_usec = us % 1000000;
    return 0;
}

int galaxia_settimeofday(const struct timeval *tv, const struct timezone *tz) {
    rtc_offset_us = (int64_t)tv->tv_sec * 1000000 + tv->tv_usec - NOW();
    return 0;
}

// ======================================================================== sleep

static struct {
    int64_t timer_us;       // -1: no timer
    int ext0_pin, ext0_level;
    uint64_t ext1_mask;
    int ext1_mode;
    bool touch;
} wakeup = { -1, -1, 0, 0, 0, false };

esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t source) {
    if (source == ESP_SLEEP_WAKEUP_ALL) {
        wakeup.timer_us = -1;
        wakeup.ext0_pin = -1;
        wakeup.ext1_mask = 0;
        wakeup.touch = false;
    } else if (source == ESP_SLEEP_WAKEUP_TIMER) {
        wakeup.timer_us = -1;
    }
    return ESP_OK;
}

esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us) {
    wakeup.timer_us = time_in_us;
    return ESP_OK;
}

// Only the RTC pads (0-21) wake the chip
esp_err_t esp_sleep_enable_ext0_wakeup(gpio_num_t gpio_num, int level) {
    if (!rtc_gpio_is_valid_gpio(gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    wakeup.ext0_pin = gpio_num;
    wakeup.ext0_level = level;
    return ESP_OK;
}

esp_err_t esp_sleep_enable_ext1_wakeup(uint64_t io_mask, esp_sleep_ext1_wakeup_mode_t level_mode) {
    if (io_mask >> 22) {
        return ESP_ERR_INVALID_ARG;
    }
    wakeup.ext1_mask = io_mask;
    wakeup.ext1_mode = level_mode;
    return ESP_OK;
}

esp_err_t esp_sleep_enable_touchpad_wakeup(void) {
    wakeup.touch = true;
    return ESP_OK;
}

esp_err_t esp_sleep_enable_ulp_wakeup(void) {
    return ESP_ERR_NOT_SUPPORTED;
}

static int pad_now(int pin) {
    int v = galaxia_js_gpio_get(pin);
    if (v < 0) {
        int pulled;
        v = gpio_shim_pulled(pin, &pulled) ? pulled : 0;
    }
    return v;
}

// The cause that ends a sleep now, ESP_SLEEP_WAKEUP_UNDEFINED if none
static esp_sleep_wakeup_cause_t wake_now(void) {
    if (wakeup.ext0_pin >= 0 && pad_now(wakeup.ext0_pin) == wakeup.ext0_level) {
        return ESP_SLEEP_WAKEUP_EXT0;
    }
    if (wakeup.ext1_mask) {
        bool any_high = false, all_low = true;
        for (int pin = 0; pin < 22; pin++) {
            if (wakeup.ext1_mask & (1ULL << pin)) {
                int v = pad_now(pin);
                any_high |= v;
                all_low &= !v;
            }
        }
        if (wakeup.ext1_mode == ESP_EXT1_WAKEUP_ANY_HIGH ? any_high : all_low) {
            return ESP_SLEEP_WAKEUP_EXT1;
        }
    }
    if (wakeup.touch) {
        for (int pad = 1; pad < SOC_TOUCH_SENSOR_NUM; pad++) {
            if (galaxia_js_touch_get(pad) > 0) {
                return ESP_SLEEP_WAKEUP_TOUCHPAD;
            }
        }
    }
    return ESP_SLEEP_WAKEUP_UNDEFINED;
}

// Light sleep: the CPU stops (no timer, no thread runs) until a wake-up source.
// Without any, it returns at once.
esp_err_t esp_light_sleep_start(void) {
    if (wakeup.timer_us < 0 && wakeup.ext0_pin < 0 && !wakeup.ext1_mask && !wakeup.touch) {
        return ESP_OK;
    }
    const int64_t until = wakeup.timer_us >= 0 ? NOW() + wakeup.timer_us : INT64_MAX;
    for (;;) {
        esp_sleep_wakeup_cause_t cause = wake_now();
        if (cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
            wake_cause = cause;
            return ESP_OK;
        }
        if (NOW() >= until) {
            wake_cause = ESP_SLEEP_WAKEUP_TIMER;
            return ESP_OK;
        }
        int64_t left = (until - NOW()) / 1000;
        galaxia_js_sleep(left > 10 ? 10 : left < 1 ? 1 : (int)left);
    }
}

// Deep sleep: the chip powers down and boots again on the timer (the page wakes
// it); the RTC clock and memory are kept
void esp_deep_sleep_start(void) {
    board_reset(ESP_RST_DEEPSLEEP, ESP_SLEEP_WAKEUP_TIMER, wakeup.timer_us >= 0 ? wakeup.timer_us / 1000 : -1);
}

esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void) {
    return wake_cause;
}

// ===================================================================== watchdog

struct galaxia_wdt_user {
    int64_t fed;
};

static struct {
    uint32_t timeout_ms;
    struct galaxia_wdt_user user;
    bool has_user;
    esp_timer_handle_t timer;
} wdt;

static void wdt_check(void *arg) {
    if (wdt.has_user && NOW() - wdt.user.fed > (int64_t)wdt.timeout_ms * 1000) {
        // trigger_panic: the board resets
        board_reset(ESP_RST_TASK_WDT, ESP_SLEEP_WAKEUP_UNDEFINED, 0);
    }
}

esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config) {
    wdt.timeout_ms = config->timeout_ms;
    if (!wdt.timer) {
        const esp_timer_create_args_t args = { .callback = wdt_check, .name = "task_wdt" };
        esp_timer_create(&args, &wdt.timer);
        esp_timer_start_periodic(wdt.timer, 50 * 1000);
    }
    return ESP_OK;
}

esp_err_t esp_task_wdt_add_user(const char *user_name, esp_task_wdt_user_handle_t *user_handle_ret) {
    if (wdt.has_user) {
        return ESP_ERR_INVALID_ARG;
    }
    wdt.has_user = true;
    wdt.user.fed = NOW();
    *user_handle_ret = &wdt.user;
    return ESP_OK;
}

esp_err_t esp_task_wdt_reset_user(esp_task_wdt_user_handle_t user_handle) {
    if (!user_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    user_handle->fed = NOW();
    return ESP_OK;
}

// ============================================================ timers (timer_ll)

struct galaxia_timer_group {
    int group;
};
static timg_dev_t timer_groups[SOC_TIMER_GROUPS] = { { 0 }, { 1 } };

#define TIMER_IRQ(group, timer) (1000 + (group) * 2 + (timer))
const timer_group_signal_conn_t timer_group_periph_signals = {
    .groups = { { .timer_irq_id = { TIMER_IRQ(0, 0), TIMER_IRQ(0, 1) } }, { .timer_irq_id = { TIMER_IRQ(1, 0), TIMER_IRQ(1, 1) } } },
};

static struct hw_timer {
    bool counting, alarm_en, auto_reload;
    uint32_t divider;
    uint64_t base_count;        // the counter at base_us
    int64_t base_us;
    uint64_t alarm, reload;
    esp_timer_handle_t clock;   // fires at the alarm
} hw_timer[SOC_TIMER_GROUPS][SOC_TIMER_GROUP_TIMERS_PER_GROUP];

static struct {
    uint32_t intr_en, intr_status;
    void (*isr[SOC_TIMER_GROUP_TIMERS_PER_GROUP])(void *);
    void *isr_arg[SOC_TIMER_GROUP_TIMERS_PER_GROUP];
} hw_group[SOC_TIMER_GROUPS];

static uint64_t hw_counter(struct hw_timer *t) {
    if (!t->counting || !t->divider) {
        return t->base_count;
    }
    return t->base_count + (uint64_t)(NOW() - t->base_us) * (APB_CLK_FREQ / 1000000) / t->divider;
}

static void hw_timer_arm(int g, int i);

static void hw_timer_fire(void *arg) {
    const int g = (intptr_t)arg / 2, i = (intptr_t)arg % 2;
    struct hw_timer *t = &hw_timer[g][i];
    if (!t->counting || !t->alarm_en) {
        return;
    }
    if (hw_counter(t) < t->alarm) {
        hw_timer_arm(g, i);     // early (the clock's granularity)
        return;
    }
    t->alarm_en = false;        // the hardware clears the alarm enable
    hw_group[g].intr_status |= TIMER_LL_EVENT_ALARM(i);
    if (t->auto_reload) {
        t->base_count = t->reload;
        t->base_us = NOW();
    }
    if ((hw_group[g].intr_en & TIMER_LL_EVENT_ALARM(i)) && hw_group[g].isr[i]) {
        hw_group[g].isr[i](hw_group[g].isr_arg[i]);
    }
}

static void hw_timer_arm(int g, int i) {
    struct hw_timer *t = &hw_timer[g][i];
    if (!t->clock) {
        const esp_timer_create_args_t args = { .callback = hw_timer_fire, .arg = (void *)(intptr_t)(g * 2 + i), .name = "timg" };
        esp_timer_create(&args, &t->clock);
    }
    esp_timer_stop(t->clock);
    if (t->counting && t->alarm_en && t->divider) {
        const uint64_t now = hw_counter(t);
        const uint64_t ticks = t->alarm > now ? t->alarm - now : 0;
        esp_timer_start_once(t->clock, ticks * t->divider / (APB_CLK_FREQ / 1000000));
    }
}

void timer_hal_init(timer_hal_context_t *hal, uint32_t group_num, uint32_t timer_num) {
    hal->dev = &timer_groups[group_num];
    hal->timer_id = timer_num;
}

void timer_hal_set_counter_value(timer_hal_context_t *hal, uint64_t load_val) {
    struct hw_timer *t = &hw_timer[hal->dev->group][hal->timer_id];
    t->base_count = load_val;
    t->base_us = NOW();
    hw_timer_arm(hal->dev->group, hal->timer_id);
}

void timer_ll_enable_counter(timg_dev_t *hw, uint32_t timer_num, bool en) {
    struct hw_timer *t = &hw_timer[hw->group][timer_num];
    if (en && !t->counting) {
        t->base_us = NOW();
    } else if (!en && t->counting) {
        t->base_count = hw_counter(t);
    }
    t->counting = en;
    hw_timer_arm(hw->group, timer_num);
}

void timer_ll_set_clock_source(timg_dev_t *hw, uint32_t timer_num, gptimer_clock_source_t clk_src) {
}

void timer_ll_set_clock_prescale(timg_dev_t *hw, uint32_t timer_num, uint32_t divider) {
    struct hw_timer *t = &hw_timer[hw->group][timer_num];
    t->base_count = hw_counter(t);
    t->base_us = NOW();
    t->divider = divider;
}

void timer_ll_set_count_direction(timg_dev_t *hw, uint32_t timer_num, gptimer_count_direction_t direction) {
}

void timer_ll_enable_intr(timg_dev_t *hw, uint32_t mask, bool en) {
    if (en) {
        hw_group[hw->group].intr_en |= mask;
    } else {
        hw_group[hw->group].intr_en &= ~mask;
    }
}

void timer_ll_clear_intr_status(timg_dev_t *hw, uint32_t mask) {
    hw_group[hw->group].intr_status &= ~mask;
}

uint32_t timer_ll_get_intr_status(timg_dev_t *hw) {
    return hw_group[hw->group].intr_status;
}

void timer_ll_set_alarm_value(timg_dev_t *hw, uint32_t timer_num, uint64_t alarm_value) {
    hw_timer[hw->group][timer_num].alarm = alarm_value;
    hw_timer_arm(hw->group, timer_num);
}

void timer_ll_enable_alarm(timg_dev_t *hw, uint32_t timer_num, bool en) {
    hw_timer[hw->group][timer_num].alarm_en = en;
    hw_timer_arm(hw->group, timer_num);
}

void timer_ll_set_reload_value(timg_dev_t *hw, uint32_t timer_num, uint64_t load_val) {
    hw_timer[hw->group][timer_num].reload = load_val;
}

void timer_ll_enable_auto_reload(timg_dev_t *hw, uint32_t timer_num, bool en) {
    hw_timer[hw->group][timer_num].auto_reload = en;
}

uint64_t timer_ll_get_counter_value(timg_dev_t *hw, uint32_t timer_num) {
    return hw_counter(&hw_timer[hw->group][timer_num]);
}

void timer_ll_trigger_soft_capture(timg_dev_t *hw, uint32_t timer_num) {
}

// Interrupt allocation: the timer groups' alarms
struct galaxia_intr {
    int group, timer;
};

esp_err_t esp_intr_alloc(int source, int flags, void (*handler)(void *), void *arg, intr_handle_t *ret_handle) {
    const int id = source - 1000;
    if (id < 0 || id >= SOC_TIMER_GROUPS * SOC_TIMER_GROUP_TIMERS_PER_GROUP) {
        return ESP_ERR_NOT_FOUND;
    }
    const int g = id / 2, i = id % 2;
    hw_group[g].isr[i] = handler;
    hw_group[g].isr_arg[i] = arg;
    *ret_handle = calloc(1, sizeof(**ret_handle));
    (*ret_handle)->group = g;
    (*ret_handle)->timer = i;
    return ESP_OK;
}

esp_err_t esp_intr_free(intr_handle_t handle) {
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    hw_group[handle->group].isr[handle->timer] = NULL;
    free(handle);
    return ESP_OK;
}

// ========================================================================= UART

static struct {
    bool installed;
    uart_config_t cfg;
    uint32_t clk_div;           // the baud rate's divider, 1/16 (12.4 fixed point)
    QueueHandle_t queue;
    int64_t tx_done_at;
} uart[UART_NUM_MAX];

static esp_err_t uart_check(uart_port_t n) {
    return n >= 0 && n < UART_NUM_MAX ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t uart_driver_install(uart_port_t n, int rx_buffer_size, int tx_buffer_size, int queue_size, QueueHandle_t *uart_queue, int intr_alloc_flags) {
    if (uart_check(n) != ESP_OK || rx_buffer_size <= SOC_UART_FIFO_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (uart[n].installed) {
        return ESP_FAIL;
    }
    uart[n].installed = true;
    uart[n].queue = NULL;
    if (queue_size && uart_queue) {
        uart[n].queue = xQueueCreate(queue_size, sizeof(uart_event_t));
        *uart_queue = uart[n].queue;
    }
    return ESP_OK;
}

esp_err_t uart_driver_delete(uart_port_t n) {
    if (uart_check(n) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    if (uart[n].installed && uart[n].queue) {
        vQueueDelete(uart[n].queue);
    }
    uart[n].installed = false;
    return ESP_OK;
}

bool uart_is_driver_installed(uart_port_t n) {
    return uart_check(n) == ESP_OK && uart[n].installed;
}

esp_err_t uart_set_baudrate(uart_port_t n, uint32_t baudrate) {
    if (uart_check(n) != ESP_OK || !baudrate) {
        return ESP_ERR_INVALID_ARG;
    }
    // As the S2's UART: APB clock / (divider in 1/16)
    uart[n].clk_div = ((uint64_t)APB_CLK_FREQ << 4) / baudrate;
    uart[n].cfg.baud_rate = baudrate;
    return ESP_OK;
}

esp_err_t uart_get_baudrate(uart_port_t n, uint32_t *baudrate) {
    if (uart_check(n) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!uart[n].clk_div) {
        uart_set_baudrate(n, 115200);
    }
    *baudrate = ((uint64_t)APB_CLK_FREQ << 4) / uart[n].clk_div;
    return ESP_OK;
}

esp_err_t uart_param_config(uart_port_t n, const uart_config_t *cfg) {
    if (uart_check(n) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    uart[n].cfg = *cfg;
    return uart_set_baudrate(n, cfg->baud_rate);
}

esp_err_t uart_set_pin(uart_port_t n, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num) {
    if (uart_check(n) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((tx_io_num >= 0 && !GPIO_IS_VALID_OUTPUT_GPIO(tx_io_num)) || (rx_io_num >= 0 && !GPIO_IS_VALID_GPIO(rx_io_num))) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

#define UART_FIELD(name, field, type) \
    esp_err_t uart_set_##name(uart_port_t n, type v) { \
        if (uart_check(n) != ESP_OK) return ESP_ERR_INVALID_ARG; \
        uart[n].cfg.field = v; \
        return ESP_OK; \
    } \
    esp_err_t uart_get_##name(uart_port_t n, type *v) { \
        if (uart_check(n) != ESP_OK) return ESP_ERR_INVALID_ARG; \
        *v = uart[n].cfg.field; \
        return ESP_OK; \
    }
UART_FIELD(word_length, data_bits, uart_word_length_t)
UART_FIELD(parity, parity, uart_parity_t)
UART_FIELD(stop_bits, stop_bits, uart_stop_bits_t)

esp_err_t uart_set_rx_full_threshold(uart_port_t n, int threshold) {
    return uart_check(n);
}

esp_err_t uart_set_rx_timeout(uart_port_t n, const uint8_t tout_thresh) {
    return uart_check(n);
}

esp_err_t uart_set_line_inverse(uart_port_t n, uint32_t inverse_mask) {
    return uart_check(n);
}

esp_err_t uart_set_hw_flow_ctrl(uart_port_t n, uart_hw_flowcontrol_t flow_ctrl, uint8_t rx_thresh) {
    if (uart_check(n) != ESP_OK || flow_ctrl >= UART_HW_FLOWCTRL_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    uart[n].cfg.flow_ctrl = flow_ctrl;
    return ESP_OK;
}

esp_err_t uart_get_buffered_data_len(uart_port_t n, size_t *size) {
    if (uart_check(n) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    *size = 0;      // nothing on RX
    return ESP_OK;
}

esp_err_t uart_flush_input(uart_port_t n) {
    return uart_check(n);
}

// A character on the line: start bit, data bits, parity, stop bits
static int64_t uart_char_us(uart_port_t n) {
    const uart_config_t *c = &uart[n].cfg;
    const int bits = 1 + (5 + c->data_bits) + (c->parity ? 1 : 0) + (c->stop_bits == UART_STOP_BITS_2 ? 2 : 1);
    uint32_t baud;
    uart_get_baudrate(n, &baud);
    return (int64_t)bits * 1000000 / (baud ? baud : 1);
}

int uart_write_bytes(uart_port_t n, const void *src, size_t size) {
    if (uart_check(n) != ESP_OK || !uart[n].installed) {
        return -1;
    }
    const int64_t start = uart[n].tx_done_at > NOW() ? uart[n].tx_done_at : NOW();
    uart[n].tx_done_at = start + (int64_t)size * uart_char_us(n);
    return size;
}

esp_err_t uart_wait_tx_done(uart_port_t n, TickType_t ticks_to_wait) {
    if (uart_check(n) != ESP_OK || !uart[n].installed) {
        return ESP_FAIL;
    }
    const int64_t until = NOW() + (int64_t)ticks_to_wait * portTICK_PERIOD_MS * 1000;
    while (NOW() < uart[n].tx_done_at) {
        if (NOW() >= until) {
            return ESP_ERR_TIMEOUT;
        }
        int64_t left = (uart[n].tx_done_at - NOW()) / 1000;
        galaxia_thread_sleep_ms(left > 10 ? 10 : left < 1 ? 1 : (int)left);
    }
    return ESP_OK;
}

int uart_read_bytes(uart_port_t n, void *buf, uint32_t length, TickType_t ticks_to_wait) {
    if (uart_check(n) != ESP_OK || !uart[n].installed) {
        return -1;
    }
    // Nothing is wired to RX: the wait times out
    const int64_t until = NOW() + (int64_t)ticks_to_wait * portTICK_PERIOD_MS * 1000;
    while (NOW() < until) {
        int64_t left = (until - NOW()) / 1000;
        galaxia_thread_sleep_ms(left > 10 ? 10 : left < 1 ? 1 : (int)left);
    }
    return 0;
}

esp_err_t uart_get_sclk_freq(uart_sclk_t sclk, uint32_t *out_freq_hz) {
    *out_freq_hz = sclk == UART_SCLK_REF_TICK ? REF_CLK_FREQ : APB_CLK_FREQ;
    return ESP_OK;
}

// ========================================================================== RMT

// The bitstream channel (esp32.RMT.bitstream_channel()): the board's default
int8_t esp32_rmt_bitstream_channel_id = RMT_CHANNEL_MAX - 1;

static struct {
    rmt_config_t cfg;
    bool installed;
    sample_to_rmt_t translator;
} rmt[RMT_CHANNEL_MAX];

esp_err_t rmt_config(const rmt_config_t *param) {
    if (param->channel < 0 || param->channel >= RMT_CHANNEL_MAX || !GPIO_IS_VALID_OUTPUT_GPIO(param->gpio_num)) {
        return ESP_ERR_INVALID_ARG;
    }
    rmt[param->channel].cfg = *param;
    return ESP_OK;
}

esp_err_t rmt_driver_install_core1(uint8_t channel_id) {
    if (channel_id >= RMT_CHANNEL_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (rmt[channel_id].installed) {
        return ESP_ERR_INVALID_STATE;
    }
    rmt[channel_id].installed = true;
    return ESP_OK;
}

esp_err_t rmt_driver_uninstall(rmt_channel_t channel) {
    if (channel < 0 || channel >= RMT_CHANNEL_MAX || !rmt[channel].installed) {
        return ESP_ERR_INVALID_STATE;
    }
    rmt[channel].installed = false;
    return ESP_OK;
}

esp_err_t rmt_get_counter_clock(rmt_channel_t channel, uint32_t *clock_hz) {
    if (channel < 0 || channel >= RMT_CHANNEL_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    *clock_hz = APB_CLK_FREQ / (rmt[channel].cfg.clk_div ? rmt[channel].cfg.clk_div : 1);
    return ESP_OK;
}

esp_err_t rmt_translator_init(rmt_channel_t channel, sample_to_rmt_t fn) {
    if (channel < 0 || channel >= RMT_CHANNEL_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    rmt[channel].translator = fn;
    return ESP_OK;
}

// The bits leave on the pad; the page may show them (NeoPixels on a pin)
esp_err_t rmt_write_sample(rmt_channel_t channel, const uint8_t *src, size_t src_size, bool wait_tx_done) {
    if (channel < 0 || channel >= RMT_CHANNEL_MAX || !rmt[channel].installed) {
        return ESP_ERR_INVALID_STATE;
    }
    EM_ASM({ Module.galaxia.bitstream && Module.galaxia.bitstream($0, HEAPU8.slice($1, $1 + $2)); },
        rmt[channel].cfg.gpio_num, src, src_size);
    // 1.25 us a bit (800 kHz NeoPixels)
    if (wait_tx_done) {
        mp_hal_delay_us(src_size * 10);
    }
    return ESP_OK;
}

esp_err_t rmt_wait_tx_done(rmt_channel_t channel, TickType_t wait_time) {
    return channel >= 0 && channel < RMT_CHANNEL_MAX ? ESP_OK : ESP_ERR_INVALID_ARG;
}
