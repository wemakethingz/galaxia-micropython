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
#include "mpconfigboard.h"

extern int galaxia_js_gpio_get(int pin);
extern int galaxia_js_gpio_next_changed(void);
extern void galaxia_js_screen(const uint8_t *rgba, int width, int height);
extern void galaxia_js_sleep(int ms);

const char *esp_err_to_name(esp_err_t code) {
    return code == ESP_OK ? "ESP_OK" : "ESP_FAIL";
}

// ---- time ----

int64_t esp_timer_get_time(void) {
    return (int64_t)(emscripten_get_now() * 1000.0);
}

TickType_t xTaskGetTickCount(void) {
    return (TickType_t)(esp_timer_get_time() / 1000 / portTICK_PERIOD_MS);
}

void vTaskDelay(TickType_t ticks) {
    // Only used by drivers during init (screen reset): a real pause, nothing else runs
    galaxia_js_sleep(ticks * portTICK_PERIOD_MS);
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
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t ticks) {
    (void)ticks;
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

void esp_restart(void) {
    EM_ASM({ Module.galaxia.restart(); });
}

// ---- GPIO ----

#define GPIO_COUNT 64

static struct {
    bool output;
    uint8_t level;          // output level
    gpio_int_type_t intr_type;
    bool intr_enabled;
    gpio_isr_t handler;
    void *arg;
} gpio[GPIO_COUNT];

static bool pin_ok(gpio_num_t pin) {
    return pin >= 0 && pin < GPIO_COUNT;
}

esp_err_t gpio_config(const gpio_config_t *conf) {
    for (int pin = 0; pin < GPIO_COUNT; pin++) {
        if (conf->pin_bit_mask & (1ULL << pin)) {
            gpio[pin].output = conf->mode & GPIO_MODE_OUTPUT;
            gpio[pin].intr_type = conf->intr_type;
            gpio[pin].intr_enabled = conf->intr_type != GPIO_INTR_DISABLE;
        }
    }
    return ESP_OK;
}

esp_err_t gpio_reset_pin(gpio_num_t pin) {
    if (pin_ok(pin)) {
        gpio[pin].output = false;
        gpio[pin].intr_type = GPIO_INTR_DISABLE;
        gpio[pin].intr_enabled = false;
    }
    return ESP_OK;
}

esp_err_t gpio_set_direction(gpio_num_t pin, gpio_mode_t mode) {
    if (pin_ok(pin)) {
        gpio[pin].output = mode & GPIO_MODE_OUTPUT;
        peripherals_gpio_direction(pin, mode);
    }
    return ESP_OK;
}

esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) {
    if (pin_ok(pin)) {
        gpio[pin].level = level ? 1 : 0;
        if (pin == MICROPY_THINGZ_SCREEN_RESET_GPIO && !level) {
            st7735_reset();
        }
    }
    return ESP_OK;
}

int gpio_get_level(gpio_num_t pin) {
    if (!pin_ok(pin)) {
        return 0;
    }
    return gpio[pin].output ? gpio[pin].level : galaxia_js_gpio_get(pin);
}

esp_err_t gpio_set_pull_mode(gpio_num_t pin, gpio_pull_mode_t pull) {
    return ESP_OK;
}

esp_err_t gpio_set_intr_type(gpio_num_t pin, gpio_int_type_t type) {
    if (pin_ok(pin)) {
        gpio[pin].intr_type = type;
    }
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

esp_err_t gpio_isr_handler_add(gpio_num_t pin, gpio_isr_t handler, void *arg) {
    if (pin_ok(pin)) {
        gpio[pin].handler = handler;
        gpio[pin].arg = arg;
    }
    return ESP_OK;
}

esp_err_t gpio_isr_handler_remove(gpio_num_t pin) {
    if (pin_ok(pin)) {
        gpio[pin].handler = NULL;
    }
    return ESP_OK;
}

// The page changed input levels: edges go to the interrupt handlers
static void deliver_gpio_edges(void) {
    int pin;
    while ((pin = galaxia_js_gpio_next_changed()) >= 0) {
        if (!pin_ok(pin) || gpio[pin].output || !gpio[pin].handler || !gpio[pin].intr_enabled) {
            continue;
        }
        const int level = galaxia_js_gpio_get(pin);
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
}

// ---- SPI master: the only device is the screen ----

struct galaxia_spi_device {
    int queued_count;
    spi_transaction_t *queued[8];
};

esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t *cfg, int dma_chan) {
    return ESP_OK;
}

esp_err_t spi_bus_add_device(spi_host_device_t host, const spi_device_interface_config_t *cfg, spi_device_handle_t *out) {
    *out = calloc(1, sizeof(**out));
    return ESP_OK;
}

esp_err_t spi_device_polling_transmit(spi_device_handle_t dev, spi_transaction_t *t) {
    const uint8_t *bytes = (t->flags & SPI_TRANS_USE_TXDATA) ? t->tx_data : t->tx_buffer;
    if (bytes && t->length) {
        st7735_write(gpio[MICROPY_THINGZ_SCREEN_DC_GPIO].level, bytes, t->length / 8);
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
    run_timers();
    int64_t now = esp_timer_get_time();
    if (st7735_frame() != sent_frame && now - sent_at >= SCREEN_MIN_PERIOD_MS * 1000) {
        sent_frame = st7735_frame();
        sent_at = now;
        galaxia_js_screen(st7735_picture(), ST7735_PICTURE_W, ST7735_PICTURE_H);
    }
    busy = false;
}
