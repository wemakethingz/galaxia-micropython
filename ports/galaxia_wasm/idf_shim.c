// ESP-IDF / FreeRTOS shim for the WebAssembly build (declarations in idf/idf_all.h).
//
// One thread: what runs in a FreeRTOS task or an esp_timer task on the board runs
// here from idf_shim_poll(), called by the VM hook, sleeps and the REPL's wait for
// input. Interrupt handlers of GPIO inputs run from there too, when the page
// changes a level. The SPI bus goes to the ST7735 model.

#include <string.h>
#include <emscripten.h>

#include "idf/idf_all.h"
#include "idf_shim.h"
#include "st7735.h"
#include "peripherals.h"
#include "net.h"
#include "mpthreadport.h"
#include "mpconfigboard.h"

extern int galaxia_js_gpio_get(int pin);
extern int galaxia_js_gpio_next_changed(void);
extern void galaxia_js_screen(const uint8_t *rgba, int width, int height);
extern void galaxia_js_sleep(int ms);

// The names ESP-IDF gives the errors (OSError messages of check_esp_err)
const char *esp_err_to_name(esp_err_t code) {
    static const struct { esp_err_t code; const char *name; } names[] = {
        { ESP_OK, "ESP_OK" }, { ESP_FAIL, "ESP_FAIL" }, { ESP_ERR_NO_MEM, "ESP_ERR_NO_MEM" },
        { ESP_ERR_INVALID_ARG, "ESP_ERR_INVALID_ARG" }, { ESP_ERR_INVALID_STATE, "ESP_ERR_INVALID_STATE" },
        { ESP_ERR_INVALID_SIZE, "ESP_ERR_INVALID_SIZE" }, { ESP_ERR_NOT_FOUND, "ESP_ERR_NOT_FOUND" },
        { ESP_ERR_NOT_SUPPORTED, "ESP_ERR_NOT_SUPPORTED" }, { ESP_ERR_TIMEOUT, "ESP_ERR_TIMEOUT" },
        { ESP_ERR_INVALID_RESPONSE, "ESP_ERR_INVALID_RESPONSE" }, { ESP_ERR_INVALID_CRC, "ESP_ERR_INVALID_CRC" },
        { ESP_ERR_INVALID_VERSION, "ESP_ERR_INVALID_VERSION" }, { ESP_ERR_INVALID_MAC, "ESP_ERR_INVALID_MAC" },
        { ESP_ERR_NOT_FINISHED, "ESP_ERR_NOT_FINISHED" },
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (names[i].code == code) {
            return names[i].name;
        }
    }
    return "UNKNOWN ERROR";
}

// ---- time ----

// The browser's clock is coarse in a Worker without cross-origin isolation (100 us in
// Chrome, 1 ms in Firefox): between its ticks the time still moves on, by 1 us at
// each read, as a CPU spinning on its timer sees it, at most one tick ahead (code
// that times something short in a loop, as thingz_led's light measurement, works)
int64_t esp_timer_get_time(void) {
    static int64_t last;
    const int64_t real = (int64_t)(emscripten_get_now() * 1000.0);
    int64_t t = real > last ? real : last + 1;
    if (t > real + 1000) {
        t = last;
    }
    last = t;
    return t;
}

TickType_t xTaskGetTickCount(void) {
    return (TickType_t)(esp_timer_get_time() / 1000 / portTICK_PERIOD_MS);
}

void vTaskDelay(TickType_t ticks) {
    // The caller waits, the other tasks and threads run meanwhile (the debug menu's
    // task, which debug_mode_stop() waits for); alone, a real pause. A sleep can end
    // early (an input wakes the sleepers): wait again until the deadline.
    const int64_t until = esp_timer_get_time() + (int64_t)ticks * portTICK_PERIOD_MS * 1000;
    for (int64_t now = esp_timer_get_time(); now < until; now = esp_timer_get_time()) {
        galaxia_thread_sleep_ms((int)((until - now + 999) / 1000));
    }
}

// ---- semaphores and queues: one thread, so a take either succeeds now or fails ----

struct galaxia_semaphore {
    int count;
    int max;
};

static SemaphoreHandle_t semaphore_new(int max) {
    SemaphoreHandle_t s = calloc(1, sizeof(*s));
    s->max = max;
    return s;
}

SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    return semaphore_new(1);
}

SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    SemaphoreHandle_t s = semaphore_new(1);
    s->count = 1;
    return s;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks) {
    (void)ticks;
    if (s && s->count > 0) {
        s->count--;
        return pdTRUE;
    }
    return pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t s) {
    if (s && s->count < s->max) {
        s->count++;
        return pdTRUE;
    }
    return pdFALSE;
}

void vSemaphoreDelete(SemaphoreHandle_t s) {
    free(s);
}

struct galaxia_queue {
    UBaseType_t len, item_size, head, count;
    uint8_t *items;
    void *waiter;       // the task blocked on the queue, woken by a send
};

QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t item_size) {
    QueueHandle_t q = calloc(1, sizeof(*q));
    q->len = len;
    q->item_size = item_size;
    q->items = calloc(len, item_size);
    return q;
}

BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t ticks) {
    (void)ticks;
    if (!q || q->count == q->len) {
        return pdFALSE;
    }
    memcpy(q->items + ((q->head + q->count) % q->len) * q->item_size, item, q->item_size);
    q->count++;
    if (q->waiter) {
        galaxia_task_wake(q->waiter);
    }
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t ticks) {
    // A FreeRTOS task (UART events, the system menu) waits for an item: blocked
    // until a send wakes it or the timeout, as in FreeRTOS (it does not run
    // meanwhile). Elsewhere the senders have already run (one thread), so the queue
    // holds what will ever come.
    if (q && q->count == 0 && ticks && galaxia_in_task()) {
        const int64_t until = ticks == portMAX_DELAY ? INT64_MAX : esp_timer_get_time() + (int64_t)ticks * portTICK_PERIOD_MS * 1000;
        for (int64_t now = esp_timer_get_time(); q->count == 0 && now < until; now = esp_timer_get_time()) {
            int64_t left = (until - now + 999) / 1000;
            q->waiter = galaxia_task_self();
            galaxia_thread_sleep_ms(left > INT32_MAX ? INT32_MAX : (int)left);
            q->waiter = NULL;
        }
    }
    if (!q || q->count == 0) {
        return pdFALSE;
    }
    memcpy(item, q->items + q->head * q->item_size, q->item_size);
    q->head = (q->head + 1) % q->len;
    q->count--;
    return pdTRUE;
}

BaseType_t xQueueReset(QueueHandle_t q) {
    if (q) {
        q->head = q->count = 0;
    }
    return pdPASS;
}

void vQueueDelete(QueueHandle_t q) {
    if (q) {
        free(q->items);
        free(q);
    }
}

// ---- esp_timer ----

struct galaxia_timer {
    esp_timer_create_args_t args;
    bool active;
    int64_t period_us;      // 0: one-shot
    int64_t due_us;
    struct galaxia_timer *next;
};

static struct galaxia_timer *timers;

esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out) {
    struct galaxia_timer *t = calloc(1, sizeof(*t));
    t->args = *args;
    t->next = timers;
    timers = t;
    *out = t;
    return ESP_OK;
}

esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t timeout_us) {
    t->active = true;
    t->period_us = 0;
    t->due_us = esp_timer_get_time() + timeout_us;
    return ESP_OK;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t period_us) {
    t->active = true;
    t->period_us = period_us;
    t->due_us = esp_timer_get_time() + period_us;
    return ESP_OK;
}

esp_err_t esp_timer_stop(esp_timer_handle_t t) {
    if (!t->active) {
        return ESP_ERR_INVALID_STATE;
    }
    t->active = false;
    return ESP_OK;
}

esp_err_t esp_timer_delete(esp_timer_handle_t t) {
    for (struct galaxia_timer **p = &timers; *p; p = &(*p)->next) {
        if (*p == t) {
            *p = t->next;
            free(t);
            break;
        }
    }
    return ESP_OK;
}

bool esp_timer_is_active(esp_timer_handle_t t) {
    return t->active;
}

int idf_shim_next_timer_ms(void) {
    int64_t now = esp_timer_get_time(), next = -1;
    for (struct galaxia_timer *t = timers; t; t = t->next) {
        if (t->active && (next < 0 || t->due_us < next)) {
            next = t->due_us;
        }
    }
    if (next < 0) {
        return -1;
    }
    return next <= now ? 0 : (int)((next - now + 999) / 1000);
}

static void run_timers(void) {
    // On the board the callbacks run in the esp_timer task, never on a Python
    // thread's stack. Here they run from the main thread's polls only: run from a
    // secondary thread's sleep (time.sleep_ms in a _thread), the screen's raw
    // refresh, which opens and reads image files, ended up calling garbage when a
    // driver task (the system menu's) ran in the middle of it.
    if (!galaxia_in_main_thread()) {
        return;
    }
    int64_t now = esp_timer_get_time();
    for (struct galaxia_timer *t = timers; t; t = t->next) {
        if (!t->active || t->due_us > now) {
            continue;
        }
        if (t->period_us) {
            // Late: skip the missed periods, as with skip_unhandled_events
            t->due_us += t->period_us;
            if (t->due_us <= now) {
                t->due_us = now + t->period_us;
            }
        } else {
            t->active = false;
        }
        t->args.callback(t->args.arg);
        // The callback may have created or deleted timers: start again next poll
        break;
    }
}

// ---- heap, system ----

size_t heap_caps_get_free_size(uint32_t caps) {
    (void)caps;
    return 1 << 20;
}

size_t esp_get_free_heap_size(void) {
    return 1 << 20;
}

// ---- GPIO ----
// The pads of the ESP32-S2 and what the Galaxia wires to them. A pad's level is what
// drives it: its own output (push-pull, or open drain pulling low), else the
// board (buttons A and B on 45 and 43 have a pull-up: 1, 0 when pressed from the
// page), else its internal pull-up or pull-down, else nothing (it reads 0). An
// input disabled reads 0. Each change of a pad's level, from the page or from the
// program, raises its interrupt as the GPIO block does. The GPIO matrix routes a
// peripheral's signal (LEDC) or the GPIO output register to the pad.

#define GPIO_COUNT 64

static struct {
    bool input, output, open_drain;
    uint8_t level;          // output register
    bool pull_up, pull_down, hold;
    int drive;
    int signal;             // output signal routed to the pad (SIG_GPIO_OUT_IDX: GPIO)
    uint8_t pad;            // last level seen (edges)
    gpio_int_type_t intr_type;
    bool intr_enabled;
    gpio_isr_t handler;
    void *arg;
} gpio[GPIO_COUNT];

static bool pin_ok(gpio_num_t pin) {
    return pin >= 0 && pin < GPIO_COUNT;
}

void gpio_shim_init(void) {
    for (int pin = 0; pin < GPIO_COUNT; pin++) {
        gpio[pin].signal = SIG_GPIO_OUT_IDX;
        gpio[pin].drive = GPIO_DRIVE_CAP_DEFAULT;
    }
}

// What the board drives on a pad, -1 for nothing
static int board_level(gpio_num_t pin) {
    int v = galaxia_js_gpio_get(pin);
    if (v >= 0) {
        return v;
    }
    if (pin == 45 || pin == 43) {
        return 1;       // button released: pull-up of the board
    }
    return -1;
}

// The level on a pad
static int pad_level(gpio_num_t pin) {
    if (gpio[pin].output && gpio[pin].signal == SIG_GPIO_OUT_IDX && !(gpio[pin].open_drain && gpio[pin].level)) {
        return gpio[pin].level;
    }
    int v = board_level(pin);
    if (v >= 0) {
        return v;
    }
    if (gpio[pin].pull_up || gpio[pin].open_drain) {
        return 1;       // (an open-drain line released: the bus pull-ups)
    }
    return 0;
}

static void gpio_edge(gpio_num_t pin) {
    const int level = pad_level(pin);
    if (level == gpio[pin].pad) {
        return;
    }
    gpio[pin].pad = level;
    if (!gpio[pin].input || !gpio[pin].handler || !gpio[pin].intr_enabled) {
        return;
    }
    bool fire;
    switch (gpio[pin].intr_type) {
        case GPIO_INTR_POSEDGE:
            fire = level;
            break;
        case GPIO_INTR_NEGEDGE:
            fire = !level;
            break;
        case GPIO_INTR_ANYEDGE:
            fire = true;
            break;
        case GPIO_INTR_LOW_LEVEL:
            fire = !level;
            break;
        case GPIO_INTR_HIGH_LEVEL:
            fire = level;
            break;
        default:
            fire = false;
            break;
    }
    if (fire) {
        gpio[pin].handler(gpio[pin].arg);
    }
}

static void set_mode(gpio_num_t pin, gpio_mode_t mode) {
    gpio[pin].input = mode & GPIO_MODE_DEF_INPUT;
    gpio[pin].output = mode & GPIO_MODE_DEF_OUTPUT;
    gpio[pin].open_drain = mode & GPIO_MODE_DEF_OD;
    peripherals_gpio_direction(pin, mode);
    gpio_edge(pin);
}

esp_err_t gpio_config(const gpio_config_t *conf) {
    for (int pin = 0; pin < GPIO_COUNT; pin++) {
        if (conf->pin_bit_mask & (1ULL << pin)) {
            if (!GPIO_IS_VALID_GPIO(pin) || ((conf->mode & GPIO_MODE_DEF_OUTPUT) && !GPIO_IS_VALID_OUTPUT_GPIO(pin))) {
                return ESP_ERR_INVALID_ARG;
            }
            gpio[pin].signal = SIG_GPIO_OUT_IDX;
            gpio[pin].pull_up = conf->pull_up_en;
            gpio[pin].pull_down = conf->pull_down_en;
            gpio[pin].intr_type = conf->intr_type;
            gpio[pin].intr_enabled = conf->intr_type != GPIO_INTR_DISABLE;
            set_mode(pin, conf->mode);
        }
    }
    return ESP_OK;
}

// As ESP-IDF: input only, pull-up, no interrupt, GPIO function
esp_err_t gpio_reset_pin(gpio_num_t pin) {
    if (!GPIO_IS_VALID_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio[pin].signal = SIG_GPIO_OUT_IDX;
    gpio[pin].pull_up = true;
    gpio[pin].pull_down = false;
    gpio[pin].intr_type = GPIO_INTR_DISABLE;
    gpio[pin].intr_enabled = false;
    set_mode(pin, GPIO_MODE_INPUT);
    return ESP_OK;
}

esp_err_t gpio_set_direction(gpio_num_t pin, gpio_mode_t mode) {
    if (!GPIO_IS_VALID_GPIO(pin) || ((mode & GPIO_MODE_DEF_OUTPUT) && !GPIO_IS_VALID_OUTPUT_GPIO(pin))) {
        return ESP_ERR_INVALID_ARG;
    }
    set_mode(pin, mode);
    return ESP_OK;
}

esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) {
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (gpio[pin].hold) {
        return ESP_OK;      // held: the pad keeps its level
    }
    gpio[pin].level = level ? 1 : 0;
    if (pin == MICROPY_THINGZ_SCREEN_RESET_GPIO && !level) {
        st7735_reset();
    }
    gpio_edge(pin);
    return ESP_OK;
}

int gpio_get_level(gpio_num_t pin) {
    if (!pin_ok(pin) || !gpio[pin].input) {
        return 0;
    }
    return pad_level(pin);
}

// For the peripherals: the output register of a pad and whether it drives it
int gpio_shim_output(gpio_num_t pin) {
    return pin_ok(pin) && gpio[pin].output && gpio[pin].signal == SIG_GPIO_OUT_IDX ? gpio[pin].level : -1;
}

int gpio_shim_signal(gpio_num_t pin) {
    return pin_ok(pin) ? gpio[pin].signal : SIG_GPIO_OUT_IDX;
}

bool gpio_shim_pulled(gpio_num_t pin, int *level) {
    if (!pin_ok(pin) || (!gpio[pin].pull_up && !gpio[pin].pull_down)) {
        return false;
    }
    *level = gpio[pin].pull_up;
    return true;
}

esp_err_t gpio_set_pull_mode(gpio_num_t pin, gpio_pull_mode_t pull) {
    if (!GPIO_IS_VALID_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio[pin].pull_up = pull == GPIO_PULLUP_ONLY || pull == GPIO_PULLUP_PULLDOWN;
    gpio[pin].pull_down = pull == GPIO_PULLDOWN_ONLY || pull == GPIO_PULLUP_PULLDOWN;
    gpio_edge(pin);
    return ESP_OK;
}

esp_err_t gpio_pullup_en(gpio_num_t pin) {
    if (!GPIO_IS_VALID_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio[pin].pull_up = true;
    gpio_edge(pin);
    return ESP_OK;
}

esp_err_t gpio_pullup_dis(gpio_num_t pin) {
    if (!GPIO_IS_VALID_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio[pin].pull_up = false;
    gpio_edge(pin);
    return ESP_OK;
}

esp_err_t gpio_pulldown_en(gpio_num_t pin) {
    if (!GPIO_IS_VALID_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio[pin].pull_down = true;
    gpio_edge(pin);
    return ESP_OK;
}

esp_err_t gpio_pulldown_dis(gpio_num_t pin) {
    if (!GPIO_IS_VALID_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio[pin].pull_down = false;
    gpio_edge(pin);
    return ESP_OK;
}

esp_err_t gpio_hold_en(gpio_num_t pin) {
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    gpio[pin].hold = true;
    return ESP_OK;
}

esp_err_t gpio_hold_dis(gpio_num_t pin) {
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    gpio[pin].hold = false;
    return ESP_OK;
}

esp_err_t gpio_set_drive_capability(gpio_num_t pin, gpio_drive_cap_t strength) {
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin) || strength >= GPIO_DRIVE_CAP_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio[pin].drive = strength;
    return ESP_OK;
}

void esp_rom_gpio_pad_select_gpio(uint32_t pin) {
    if (pin_ok(pin)) {
        gpio[pin].signal = SIG_GPIO_OUT_IDX;
        gpio_edge(pin);
    }
}

void esp_rom_gpio_connect_out_signal(uint32_t pin, uint32_t signal, bool out_inv, bool oen_inv) {
    if (pin_ok(pin)) {
        gpio[pin].signal = signal;
        gpio_edge(pin);
    }
}

// The output set/clear registers (bit-banged bitstream)
void galaxia_gpio_reg_write(uint32_t reg, uint32_t value) {
    const int base = (reg == GPIO_OUT1_W1TS_REG || reg == GPIO_OUT1_W1TC_REG) ? 32 : 0;
    const bool set = reg == GPIO_OUT_W1TS_REG || reg == GPIO_OUT1_W1TS_REG;
    for (int i = 0; i < 32; i++) {
        if ((value & (1u << i)) && pin_ok(base + i)) {
            gpio[base + i].level = set;
            gpio_edge(base + i);
        }
    }
}

gpio_dev_t *galaxia_gpio_dev(void) {
    static gpio_dev_t dev;
    for (int pin = 0; pin < SOC_GPIO_PIN_COUNT; pin++) {
        dev.pin[pin].int_type = gpio[pin].intr_enabled ? gpio[pin].intr_type : GPIO_INTR_DISABLE;
    }
    return &dev;
}

esp_err_t gpio_set_intr_type(gpio_num_t pin, gpio_int_type_t type) {
    if (!GPIO_IS_VALID_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio[pin].intr_type = type;
    return ESP_OK;
}

esp_err_t gpio_intr_enable(gpio_num_t pin) {
    if (pin_ok(pin)) {
        gpio[pin].intr_enabled = true;
    }
    return ESP_OK;
}

esp_err_t gpio_intr_disable(gpio_num_t pin) {
    if (pin_ok(pin)) {
        gpio[pin].intr_enabled = false;
    }
    return ESP_OK;
}

esp_err_t gpio_install_isr_service(int flags) {
    return ESP_OK;
}

// As ESP-IDF's: adding a handler enables the pad's interrupt
esp_err_t gpio_isr_handler_add(gpio_num_t pin, gpio_isr_t handler, void *arg) {
    if (pin_ok(pin)) {
        gpio[pin].handler = handler;
        gpio[pin].arg = arg;
        gpio[pin].intr_enabled = true;
        gpio[pin].pad = pad_level(pin);
    }
    return ESP_OK;
}

esp_err_t gpio_isr_handler_remove(gpio_num_t pin) {
    if (pin_ok(pin)) {
        gpio[pin].handler = NULL;
        gpio[pin].intr_enabled = false;
    }
    return ESP_OK;
}

bool rtc_gpio_is_valid_gpio(gpio_num_t pin) {
    return pin >= 0 && pin <= 21;
}

esp_err_t rtc_gpio_deinit(gpio_num_t pin) {
    return ESP_OK;
}

// The page changed the levels it drives: edges go to the interrupt handlers
static void deliver_gpio_edges(void) {
    int pin;
    while ((pin = galaxia_js_gpio_next_changed()) >= 0) {
        if (pin_ok(pin)) {
            gpio_edge(pin);
        }
    }
}

// ---- SPI master: the three hosts of the ESP32-S2. The screen is the device on
// SPI2 (lib/tft/ili9340.c); nothing else is wired, a device elsewhere reads 0. ----

struct galaxia_spi_device {
    spi_host_device_t host;
    bool screen;
    int queued_count;
    spi_transaction_t *queued[8];
};

static struct {
    bool initialized;
    int devices;
} spi_bus[3];

esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t *cfg, int dma_chan) {
    if (host < SPI2_HOST || host > SPI3_HOST) {
        return ESP_ERR_INVALID_ARG;     // SPI1 is the flash's
    }
    if (spi_bus[host].initialized) {
        return ESP_ERR_INVALID_STATE;   // "SPI host already in use"
    }
    spi_bus[host].initialized = true;
    return ESP_OK;
}

esp_err_t spi_bus_free(spi_host_device_t host) {
    if (host < SPI2_HOST || host > SPI3_HOST || !spi_bus[host].initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (spi_bus[host].devices) {
        return ESP_ERR_INVALID_STATE;
    }
    spi_bus[host].initialized = false;
    return ESP_OK;
}

esp_err_t spi_bus_add_device(spi_host_device_t host, const spi_device_interface_config_t *cfg, spi_device_handle_t *out) {
    if (host < SPI2_HOST || host > SPI3_HOST || !spi_bus[host].initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    if (spi_bus[host].devices == 3) {
        return ESP_ERR_NOT_FOUND;       // no free CS
    }
    *out = calloc(1, sizeof(**out));
    (*out)->host = host;
    (*out)->screen = host == SPI2_HOST && cfg->spics_io_num == MICROPY_THINGZ_SCREEN_TFT_CS_GPIO && spi_bus[host].devices == 0;
    spi_bus[host].devices++;
    return ESP_OK;
}

esp_err_t spi_bus_remove_device(spi_device_handle_t dev) {
    if (!dev) {
        return ESP_ERR_INVALID_STATE;
    }
    spi_bus[dev->host].devices--;
    free(dev);
    return ESP_OK;
}

esp_err_t spi_device_acquire_bus(spi_device_handle_t dev, TickType_t wait) {
    return ESP_OK;
}

void spi_device_release_bus(spi_device_handle_t dev) {
}

// What the SPI clock can be: the APB clock divided by an integer
int spi_get_actual_clock(int fapb, int hz, int duty_cycle) {
    if (hz >= fapb) {
        return fapb;
    }
    int n = (fapb + hz - 1) / hz;
    if (n > 8192) {
        n = 8192;
    }
    return fapb / n;
}

esp_err_t spi_device_polling_transmit(spi_device_handle_t dev, spi_transaction_t *t) {
    const uint8_t *bytes = (t->flags & SPI_TRANS_USE_TXDATA) ? t->tx_data : t->tx_buffer;
    if (dev->screen) {
        if (bytes && t->length) {
            st7735_write(gpio[MICROPY_THINGZ_SCREEN_DC_GPIO].level, bytes, t->length / 8);
        }
        return ESP_OK;
    }
    // Nothing on MISO
    size_t rx_bits = t->rxlength ? t->rxlength : t->length;
    uint8_t *rx = (t->flags & SPI_TRANS_USE_RXDATA) ? t->rx_data : t->rx_buffer;
    if (rx && rx_bits) {
        memset(rx, 0, (rx_bits + 7) / 8);
    }
    return ESP_OK;
}

esp_err_t spi_device_transmit(spi_device_handle_t dev, spi_transaction_t *t) {
    return spi_device_polling_transmit(dev, t);
}

// Queued (DMA) transactions complete at once; the result queue only keeps them
esp_err_t spi_device_queue_trans(spi_device_handle_t dev, spi_transaction_t *t, TickType_t ticks) {
    spi_device_polling_transmit(dev, t);
    if (dev->queued_count < 8) {
        dev->queued[dev->queued_count++] = t;
    }
    return ESP_OK;
}

esp_err_t spi_device_get_trans_result(spi_device_handle_t dev, spi_transaction_t **t, TickType_t ticks) {
    if (dev->queued_count == 0) {
        return ESP_ERR_TIMEOUT;
    }
    *t = dev->queued[0];
    memmove(dev->queued, dev->queued + 1, --dev->queued_count * sizeof(dev->queued[0]));
    return ESP_OK;
}

// ---- NVS: in memory (a fresh board at each start) ----

#define NVS_MAX 32

static struct {
    char key[16];
    bool is_str;
    uint32_t u32;
    char *str;
    size_t len;
} nvs[NVS_MAX];

static int nvs_find(const char *key, bool create) {
    for (int i = 0; i < NVS_MAX; i++) {
        if (nvs[i].key[0] && !strncmp(nvs[i].key, key, sizeof(nvs[i].key))) {
            return i;
        }
    }
    if (create) {
        for (int i = 0; i < NVS_MAX; i++) {
            if (!nvs[i].key[0]) {
                strncpy(nvs[i].key, key, sizeof(nvs[i].key) - 1);
                return i;
            }
        }
    }
    return -1;
}

esp_err_t nvs_flash_init(void) {
    return ESP_OK;
}

esp_err_t nvs_flash_erase(void) {
    for (int i = 0; i < NVS_MAX; i++) {
        free(nvs[i].str);
    }
    memset(nvs, 0, sizeof(nvs));
    return ESP_OK;
}

esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *out) {
    *out = 1;
    return ESP_OK;
}

esp_err_t nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *v) {
    int i = nvs_find(key, false);
    if (i < 0 || nvs[i].is_str) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    *v = nvs[i].u32;
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *v) {
    uint32_t u;
    esp_err_t err = nvs_get_u32(h, key, &u);
    if (err == ESP_OK) {
        *v = u;
    }
    return err;
}

esp_err_t nvs_get_i32(nvs_handle_t h, const char *key, int32_t *v) {
    return nvs_get_u32(h, key, (uint32_t *)v);
}

esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *v, size_t *len) {
    int i = nvs_find(key, false);
    if (i < 0 || !nvs[i].is_str) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (v) {
        if (*len < nvs[i].len) {
            return ESP_ERR_INVALID_ARG;
        }
        memcpy(v, nvs[i].str, nvs[i].len);
    }
    *len = nvs[i].len;
    return ESP_OK;
}

esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *v, size_t *len) {
    return nvs_get_blob(h, key, v, len);
}

esp_err_t nvs_set_u32(nvs_handle_t h, const char *key, uint32_t v) {
    int i = nvs_find(key, true);
    if (i < 0) {
        return ESP_ERR_NO_MEM;
    }
    nvs[i].is_str = false;
    nvs[i].u32 = v;
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v) {
    return nvs_set_u32(h, key, v);
}

esp_err_t nvs_set_i32(nvs_handle_t h, const char *key, int32_t v) {
    return nvs_set_u32(h, key, (uint32_t)v);
}

esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *v, size_t len) {
    int i = nvs_find(key, true);
    if (i < 0) {
        return ESP_ERR_NO_MEM;
    }
    free(nvs[i].str);
    nvs[i].str = malloc(len);
    memcpy(nvs[i].str, v, len);
    nvs[i].len = len;
    nvs[i].is_str = true;
    return ESP_OK;
}

esp_err_t nvs_set_str(nvs_handle_t h, const char *key, const char *v) {
    return nvs_set_blob(h, key, v, strlen(v) + 1);
}

esp_err_t nvs_erase_key(nvs_handle_t h, const char *key) {
    int i = nvs_find(key, false);
    if (i < 0) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    free(nvs[i].str);
    memset(&nvs[i], 0, sizeof(nvs[i]));
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t h) {
    return ESP_OK;
}

void nvs_close(nvs_handle_t h) {
}

esp_err_t esp_vfs_spiffs_register(const esp_vfs_spiffs_conf_t *conf) {
    return ESP_OK;
}

// ---- temperature sensor ----

esp_err_t temperature_sensor_install(const temperature_sensor_config_t *cfg, temperature_sensor_handle_t *out) {
    *out = NULL;
    return ESP_OK;
}

esp_err_t temperature_sensor_uninstall(temperature_sensor_handle_t h) {
    return ESP_OK;
}

esp_err_t temperature_sensor_enable(temperature_sensor_handle_t h) {
    return ESP_OK;
}

esp_err_t temperature_sensor_disable(temperature_sensor_handle_t h) {
    return ESP_OK;
}

esp_err_t temperature_sensor_get_celsius(temperature_sensor_handle_t h, float *out) {
    *out = EM_ASM_DOUBLE({ return Module.galaxia.temperature; });
    return ESP_OK;
}

// ---- polling ----

#define SCREEN_MIN_PERIOD_MS 30

void idf_shim_poll(void) {
    static bool busy;
    static uint32_t sent_frame = (uint32_t)-1;
    static int64_t sent_at;

    if (busy) {
        return;
    }
    busy = true;
    deliver_gpio_edges();
    peripherals_poll();
    net_poll();
    run_timers();
    int64_t now = esp_timer_get_time();
    if (st7735_frame() != sent_frame && now - sent_at >= SCREEN_MIN_PERIOD_MS * 1000) {
        sent_frame = st7735_frame();
        sent_at = now;
        galaxia_js_screen(st7735_picture(), ST7735_PICTURE_W, ST7735_PICTURE_H);
    }
    busy = false;
}
