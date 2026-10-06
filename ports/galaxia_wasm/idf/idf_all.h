// The subset of ESP-IDF / FreeRTOS that the Thingz code uses, for the WebAssembly
// build. Every ESP-IDF header of this directory includes this one. The behaviour is
// in idf_shim.c: one thread, timers run from galaxia_poll(), GPIO levels from the
// page, the SPI bus drives a model of the ST7735 screen.
#ifndef GALAXIA_IDF_ALL_H
#define GALAXIA_IDF_ALL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

// ---- esp_err / attributes ----
typedef int esp_err_t;
#define ESP_OK          0
#define ESP_FAIL        -1
#define ESP_ERR_NO_MEM          0x101
#define ESP_ERR_INVALID_ARG     0x102
#define ESP_ERR_INVALID_STATE   0x103
#define ESP_ERR_NOT_FOUND       0x105
#define ESP_ERR_TIMEOUT         0x107
#define ESP_ERR_NVS_BASE        0x1100
#define ESP_ERR_NVS_NOT_FOUND   (ESP_ERR_NVS_BASE + 0x02)
#define ESP_ERROR_CHECK(x) do { (void)(x); } while (0)
// sdkconfig of the Galaxia
#define CONFIG_IDF_TARGET_ESP32S2 1

#define IRAM_ATTR
#define DMA_ATTR
#define DRAM_ATTR
#define RTC_DATA_ATTR
#define ESP_LOG_LEVEL_LOCAL(...)
const char *esp_err_to_name(esp_err_t code);

// ---- esp_log ----
// The arguments are evaluated, as on the board: the firmware calls functions inside
// log calls (ESP_LOGE(TAG, "init %d", esp_now_init())). Printed on the browser
// console when built with -DGALAXIA_IDF_LOG.
static inline void galaxia_esp_log(const char *tag, const char *fmt, ...) {
    #ifdef GALAXIA_IDF_LOG
    va_list ap;
    va_start(ap, fmt);
    printf("%s: ", tag);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    #else
    (void)tag;
    (void)fmt;
    #endif
}
#define ESP_LOGE(tag, ...) galaxia_esp_log(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) galaxia_esp_log(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) galaxia_esp_log(tag, __VA_ARGS__)
#define ESP_LOGD(tag, ...) galaxia_esp_log(tag, __VA_ARGS__)
#define ESP_LOGV(tag, ...) galaxia_esp_log(tag, __VA_ARGS__)
typedef int (*vprintf_like_t)(const char *, va_list);

// ---- FreeRTOS ----
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
#define pdFAIL  0
#define portMAX_DELAY ((TickType_t)0xffffffff)
#define configTICK_RATE_HZ 100
#define portTICK_PERIOD_MS (1000 / configTICK_RATE_HZ)
#define portTICK_RATE_MS portTICK_PERIOD_MS
#define pdMS_TO_TICKS(ms) ((TickType_t)((ms) / portTICK_PERIOD_MS))
#define portYIELD_FROM_ISR(...)
#define portENTER_CRITICAL(...)
#define portEXIT_CRITICAL(...)
#define taskENTER_CRITICAL(...)
#define taskEXIT_CRITICAL(...)
typedef struct { int dummy; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED { 0 }

typedef struct galaxia_semaphore *SemaphoreHandle_t;
typedef struct galaxia_queue *QueueHandle_t;
typedef void *TaskHandle_t;

SemaphoreHandle_t xSemaphoreCreateBinary(void);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks);
BaseType_t xSemaphoreGive(SemaphoreHandle_t s);
#define xSemaphoreGiveFromISR(s, w) xSemaphoreGive(s)
#define xSemaphoreTakeFromISR(s, w) xSemaphoreTake(s, 0)
void vSemaphoreDelete(SemaphoreHandle_t s);

QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t item_size);
BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t ticks);
BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t ticks);
void vQueueDelete(QueueHandle_t q);

void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount(void);

// ---- esp_timer ----
typedef struct galaxia_timer *esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void *arg);
typedef enum { ESP_TIMER_TASK, ESP_TIMER_ISR } esp_timer_dispatch_t;
typedef struct {
    esp_timer_cb_t callback;
    void *arg;
    esp_timer_dispatch_t dispatch_method;
    const char *name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out);
esp_err_t esp_timer_start_once(esp_timer_handle_t t, uint64_t timeout_us);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t t, uint64_t period_us);
esp_err_t esp_timer_stop(esp_timer_handle_t t);
esp_err_t esp_timer_delete(esp_timer_handle_t t);
bool esp_timer_is_active(esp_timer_handle_t t);
int64_t esp_timer_get_time(void);

// ---- heap_caps ----
#define MALLOC_CAP_DMA      (1 << 3)
#define MALLOC_CAP_8BIT     (1 << 2)
#define MALLOC_CAP_32BIT    (1 << 1)
#define MALLOC_CAP_DEFAULT  (1 << 12)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_SPIRAM   (1 << 10)
#define heap_caps_malloc(size, caps) malloc(size)
#define heap_caps_calloc(n, size, caps) calloc(n, size)
#define heap_caps_free(p) free(p)
size_t heap_caps_get_free_size(uint32_t caps);
size_t esp_get_free_heap_size(void);
void esp_restart(void);

// ---- GPIO ----
typedef int gpio_num_t;
#define GPIO_NUM_NC (-1)
typedef enum {
    GPIO_MODE_DISABLE = 0, GPIO_MODE_INPUT = 1, GPIO_MODE_OUTPUT = 2,
    GPIO_MODE_OUTPUT_OD = 6, GPIO_MODE_INPUT_OUTPUT_OD = 7, GPIO_MODE_INPUT_OUTPUT = 3,
} gpio_mode_t;
typedef enum { GPIO_PULLUP_DISABLE = 0, GPIO_PULLUP_ENABLE = 1 } gpio_pullup_t;
typedef enum { GPIO_PULLDOWN_DISABLE = 0, GPIO_PULLDOWN_ENABLE = 1 } gpio_pulldown_t;
typedef enum {
    GPIO_INTR_DISABLE = 0, GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE, GPIO_INTR_ANYEDGE,
    GPIO_INTR_LOW_LEVEL, GPIO_INTR_HIGH_LEVEL,
} gpio_int_type_t;
typedef enum { GPIO_PULLUP_ONLY, GPIO_PULLDOWN_ONLY, GPIO_PULLUP_PULLDOWN, GPIO_FLOATING } gpio_pull_mode_t;
typedef struct {
    uint64_t pin_bit_mask;
    gpio_mode_t mode;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
} gpio_config_t;
typedef void (*gpio_isr_t)(void *arg);
esp_err_t gpio_config(const gpio_config_t *conf);
esp_err_t gpio_reset_pin(gpio_num_t pin);
esp_err_t gpio_set_direction(gpio_num_t pin, gpio_mode_t mode);
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level);
int gpio_get_level(gpio_num_t pin);
esp_err_t gpio_set_pull_mode(gpio_num_t pin, gpio_pull_mode_t pull);
esp_err_t gpio_set_intr_type(gpio_num_t pin, gpio_int_type_t type);
esp_err_t gpio_intr_enable(gpio_num_t pin);
esp_err_t gpio_intr_disable(gpio_num_t pin);
esp_err_t gpio_install_isr_service(int flags);
esp_err_t gpio_isr_handler_add(gpio_num_t pin, gpio_isr_t handler, void *arg);
esp_err_t gpio_isr_handler_remove(gpio_num_t pin);
#define gpio_pad_select_gpio(pin) ((void)(pin))

// ---- SPI master ----
typedef enum { SPI1_HOST = 0, SPI2_HOST = 1, SPI3_HOST = 2 } spi_host_device_t;
#define SPI_DMA_CH_AUTO 3
#define SPI_DMA_DISABLED 0
#define SPI_TRANS_USE_RXDATA (1 << 2)
#define SPI_TRANS_USE_TXDATA (1 << 3)
#define SPI_DEVICE_NO_DUMMY (1 << 6)
typedef struct galaxia_spi_device *spi_device_handle_t;
typedef struct {
    int mosi_io_num, miso_io_num, sclk_io_num, quadwp_io_num, quadhd_io_num;
    int max_transfer_sz;
    uint32_t flags;
    int intr_flags;
} spi_bus_config_t;
typedef struct spi_transaction_t spi_transaction_t;
typedef void (*transaction_cb_t)(spi_transaction_t *trans);
typedef struct {
    uint8_t command_bits, address_bits, dummy_bits, mode;
    uint16_t duty_cycle_pos, cs_ena_pretrans;
    uint8_t cs_ena_posttrans;
    int clock_speed_hz;
    int input_delay_ns;
    int spics_io_num;
    uint32_t flags;
    int queue_size;
    transaction_cb_t pre_cb;
    transaction_cb_t post_cb;
} spi_device_interface_config_t;
struct spi_transaction_t {
    uint32_t flags;
    uint16_t cmd;
    uint64_t addr;
    size_t length;
    size_t rxlength;
    void *user;
    union {
        const void *tx_buffer;
        uint8_t tx_data[4];
    };
    union {
        void *rx_buffer;
        uint8_t rx_data[4];
    };
};
esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t *cfg, int dma_chan);
esp_err_t spi_bus_add_device(spi_host_device_t host, const spi_device_interface_config_t *cfg, spi_device_handle_t *out);
esp_err_t spi_device_transmit(spi_device_handle_t dev, spi_transaction_t *t);
esp_err_t spi_device_polling_transmit(spi_device_handle_t dev, spi_transaction_t *t);
esp_err_t spi_device_queue_trans(spi_device_handle_t dev, spi_transaction_t *t, TickType_t ticks);
esp_err_t spi_device_get_trans_result(spi_device_handle_t dev, spi_transaction_t **t, TickType_t ticks);

// ---- NVS ----
typedef uint32_t nvs_handle_t;
typedef nvs_handle_t nvs_handle;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;
esp_err_t nvs_flash_init(void);
esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *out);
esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *v);
esp_err_t nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *v);
esp_err_t nvs_get_i32(nvs_handle_t h, const char *key, int32_t *v);
esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *v, size_t *len);
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *v, size_t *len);
esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v);
esp_err_t nvs_set_u32(nvs_handle_t h, const char *key, uint32_t v);
esp_err_t nvs_set_i32(nvs_handle_t h, const char *key, int32_t v);
esp_err_t nvs_set_str(nvs_handle_t h, const char *key, const char *v);
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *v, size_t len);
esp_err_t nvs_commit(nvs_handle_t h);
void nvs_close(nvs_handle_t h);

// ---- SPIFFS / VFS (configuration only, files are in the Emscripten FS) ----
typedef struct {
    const char *base_path;
    const char *partition_label;
    size_t max_files;
    bool format_if_mount_failed;
} esp_vfs_spiffs_conf_t;
esp_err_t esp_vfs_spiffs_register(const esp_vfs_spiffs_conf_t *conf);

// ---- temperature sensor ----
typedef struct galaxia_temp_sensor *temperature_sensor_handle_t;
typedef struct { int range_min, range_max; int clk_src; } temperature_sensor_config_t;
#define TEMPERATURE_SENSOR_CONFIG_DEFAULT(min, max) { .range_min = (min), .range_max = (max), .clk_src = 0 }
esp_err_t temperature_sensor_install(const temperature_sensor_config_t *cfg, temperature_sensor_handle_t *out);
esp_err_t temperature_sensor_uninstall(temperature_sensor_handle_t h);
esp_err_t temperature_sensor_enable(temperature_sensor_handle_t h);
esp_err_t temperature_sensor_disable(temperature_sensor_handle_t h);
esp_err_t temperature_sensor_get_celsius(temperature_sensor_handle_t h, float *out);

// ---- types of disabled modules that their headers still mention ----
typedef struct {
    int gpio_num, speed_mode, channel, intr_type, timer_sel;
    uint32_t duty;
    int hpoint;
    struct { unsigned int output_invert : 1; } flags;
} ledc_channel_config_t;
typedef struct galaxia_dac_continuous *dac_continuous_handle_t;

#include "idf_periph.h"

#endif
