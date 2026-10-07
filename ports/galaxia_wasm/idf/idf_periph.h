// ESP-IDF peripheral drivers used by the Thingz modules (touch, I2C, LEDC, ADC, DAC,
// Wi-Fi/ESP-NOW), for the WebAssembly build. Included by idf_all.h; simulated by
// peripherals.c.
#ifndef GALAXIA_IDF_PERIPH_H
#define GALAXIA_IDF_PERIPH_H

typedef void (*intr_handler_t)(void *arg);
#define ESP_INTR_FLAG_IRAM (1 << 10)

// ---- touch sensor (ESP32-S2) ----
typedef int touch_pad_t;
typedef enum { TOUCH_PAD_DENOISE_BIT12, TOUCH_PAD_DENOISE_BIT10, TOUCH_PAD_DENOISE_BIT8, TOUCH_PAD_DENOISE_BIT4 } touch_pad_denoise_grade_t;
typedef enum { TOUCH_PAD_DENOISE_CAP_L0, TOUCH_PAD_DENOISE_CAP_L1, TOUCH_PAD_DENOISE_CAP_L2, TOUCH_PAD_DENOISE_CAP_L3,
               TOUCH_PAD_DENOISE_CAP_L4, TOUCH_PAD_DENOISE_CAP_L5, TOUCH_PAD_DENOISE_CAP_L6, TOUCH_PAD_DENOISE_CAP_L7 } touch_pad_denoise_cap_t;
typedef struct { touch_pad_denoise_grade_t grade; touch_pad_denoise_cap_t cap_level; } touch_pad_denoise_t;
typedef enum { TOUCH_PAD_FILTER_IIR_4, TOUCH_PAD_FILTER_IIR_8, TOUCH_PAD_FILTER_IIR_16, TOUCH_PAD_FILTER_IIR_32,
               TOUCH_PAD_FILTER_IIR_64, TOUCH_PAD_FILTER_IIR_128, TOUCH_PAD_FILTER_IIR_256, TOUCH_PAD_FILTER_JITTER } touch_filter_mode_t;
typedef enum { TOUCH_PAD_SMOOTH_OFF, TOUCH_PAD_SMOOTH_IIR_2, TOUCH_PAD_SMOOTH_IIR_4, TOUCH_PAD_SMOOTH_IIR_8 } touch_smooth_mode_t;
typedef struct {
    touch_filter_mode_t mode;
    uint32_t debounce_cnt, noise_thr, noise_neg_thr, neg_noise_limit, hysteresis_thr, jitter_step;
    touch_smooth_mode_t smh_lvl;
} touch_filter_config_t;
typedef enum { TOUCH_FSM_MODE_TIMER, TOUCH_FSM_MODE_SW } touch_fsm_mode_t;
typedef uint32_t touch_pad_intr_mask_t;
#define TOUCH_PAD_INTR_MASK_DONE     (1 << 0)
#define TOUCH_PAD_INTR_MASK_ACTIVE   (1 << 1)
#define TOUCH_PAD_INTR_MASK_INACTIVE (1 << 2)
#define TOUCH_PAD_INTR_MASK_SCAN_DONE (1 << 3)
#define TOUCH_PAD_INTR_MASK_TIMEOUT  (1 << 4)
#define TOUCH_PAD_INTR_MASK_ALL      0x1f
#define TOUCH_PAD_THRESHOLD_MAX      0x1fffff
esp_err_t touch_pad_init(void);
esp_err_t touch_pad_deinit(void);
esp_err_t touch_pad_reset(void);
esp_err_t touch_pad_config(touch_pad_t pad);
esp_err_t touch_pad_denoise_set_config(const touch_pad_denoise_t *cfg);
esp_err_t touch_pad_denoise_enable(void);
esp_err_t touch_pad_filter_set_config(const touch_filter_config_t *cfg);
esp_err_t touch_pad_filter_enable(void);
esp_err_t touch_pad_timeout_set(bool enable, uint32_t threshold);
esp_err_t touch_pad_isr_register(intr_handler_t fn, void *arg, touch_pad_intr_mask_t mask);
esp_err_t touch_pad_intr_enable(touch_pad_intr_mask_t mask);
esp_err_t touch_pad_set_fsm_mode(touch_fsm_mode_t mode);
esp_err_t touch_pad_fsm_start(void);
esp_err_t touch_pad_fsm_stop(void);
esp_err_t touch_pad_read_benchmark(touch_pad_t pad, uint32_t *value);
esp_err_t touch_pad_filter_read_smooth(touch_pad_t pad, uint32_t *value);
esp_err_t touch_pad_read_raw_data(touch_pad_t pad, uint32_t *value);
esp_err_t touch_pad_set_thresh(touch_pad_t pad, uint32_t threshold);
uint32_t touch_pad_get_status(void);
touch_pad_t touch_pad_get_current_meas_channel(void);

// ---- I2C master (legacy driver, command links) ----
typedef int i2c_port_t;
typedef enum { I2C_MODE_SLAVE, I2C_MODE_MASTER } i2c_mode_t;
typedef enum { I2C_MASTER_WRITE = 0, I2C_MASTER_READ = 1 } i2c_rw_t;
typedef enum { I2C_MASTER_ACK = 0, I2C_MASTER_NACK = 1, I2C_MASTER_LAST_NACK = 2 } i2c_ack_type_t;
typedef struct {
    i2c_mode_t mode;
    int sda_io_num, scl_io_num;
    bool sda_pullup_en, scl_pullup_en;
    union {
        struct { uint32_t clk_speed; } master;
        struct { uint8_t addr_10bit_en; uint16_t slave_addr; uint32_t maximum_speed; } slave;
    };
    uint32_t clk_flags;
} i2c_config_t;
typedef struct galaxia_i2c_cmd *i2c_cmd_handle_t;
esp_err_t i2c_param_config(i2c_port_t port, const i2c_config_t *conf);
esp_err_t i2c_driver_install(i2c_port_t port, i2c_mode_t mode, size_t rx, size_t tx, int flags);
esp_err_t i2c_driver_delete(i2c_port_t port);
esp_err_t i2c_set_timeout(i2c_port_t port, int timeout);
i2c_cmd_handle_t i2c_cmd_link_create(void);
void i2c_cmd_link_delete(i2c_cmd_handle_t cmd);
esp_err_t i2c_master_start(i2c_cmd_handle_t cmd);
esp_err_t i2c_master_stop(i2c_cmd_handle_t cmd);
esp_err_t i2c_master_write_byte(i2c_cmd_handle_t cmd, uint8_t data, bool ack_en);
esp_err_t i2c_master_write(i2c_cmd_handle_t cmd, const uint8_t *data, size_t len, bool ack_en);
esp_err_t i2c_master_read_byte(i2c_cmd_handle_t cmd, uint8_t *data, i2c_ack_type_t ack);
esp_err_t i2c_master_read(i2c_cmd_handle_t cmd, uint8_t *data, size_t len, i2c_ack_type_t ack);
esp_err_t i2c_master_cmd_begin(i2c_port_t port, i2c_cmd_handle_t cmd, TickType_t ticks);
typedef struct { uint8_t dummy[64]; } StaticSemaphore_t;
typedef StaticSemaphore_t StaticQueue_t;
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *buf);
QueueHandle_t xQueueCreateStatic(UBaseType_t len, UBaseType_t item_size, uint8_t *storage, StaticQueue_t *buf);
BaseType_t xQueueReset(QueueHandle_t q);

// ---- LEDC (PWM) and GPIO drive ----
typedef enum { LEDC_LOW_SPEED_MODE = 0, LEDC_SPEED_MODE_MAX } ledc_mode_t;
typedef enum { LEDC_TIMER_0, LEDC_TIMER_1, LEDC_TIMER_2, LEDC_TIMER_3 } ledc_timer_t;
typedef struct {
    ledc_mode_t speed_mode;
    int duty_resolution;
    ledc_timer_t timer_num;
    uint32_t freq_hz;
    int clk_cfg;
} ledc_timer_config_t;
esp_err_t ledc_channel_config(const ledc_channel_config_t *cfg);
esp_err_t ledc_timer_config(const ledc_timer_config_t *cfg);
esp_err_t ledc_stop(ledc_mode_t mode, int channel, uint32_t idle_level);
uint32_t ledc_get_freq(ledc_mode_t mode, ledc_timer_t timer);
typedef enum { GPIO_DRIVE_CAP_0, GPIO_DRIVE_CAP_1, GPIO_DRIVE_CAP_2, GPIO_DRIVE_CAP_3 } gpio_drive_cap_t;
esp_err_t gpio_set_drive_capability(gpio_num_t pin, gpio_drive_cap_t strength);

// ---- ADC (legacy driver) ----
typedef enum { ADC_WIDTH_BIT_13 = 4 } adc_bits_width_t;
typedef enum { ADC_ATTEN_DB_0, ADC_ATTEN_DB_2_5, ADC_ATTEN_DB_6, ADC_ATTEN_DB_11 } adc_atten_t;
typedef enum { ADC1_CHANNEL_0, ADC1_CHANNEL_1, ADC1_CHANNEL_2, ADC1_CHANNEL_3, ADC1_CHANNEL_4,
               ADC1_CHANNEL_5, ADC1_CHANNEL_6, ADC1_CHANNEL_7, ADC1_CHANNEL_8, ADC1_CHANNEL_9 } adc1_channel_t;
typedef enum { ADC2_CHANNEL_0, ADC2_CHANNEL_1, ADC2_CHANNEL_2, ADC2_CHANNEL_3, ADC2_CHANNEL_4,
               ADC2_CHANNEL_5, ADC2_CHANNEL_6, ADC2_CHANNEL_7, ADC2_CHANNEL_8, ADC2_CHANNEL_9 } adc2_channel_t;
esp_err_t adc1_config_width(adc_bits_width_t width);
esp_err_t adc1_config_channel_atten(adc1_channel_t channel, adc_atten_t atten);
int adc1_get_raw(adc1_channel_t channel);

// ---- DAC, continuous mode ----
typedef enum { DAC_CHANNEL_MASK_CH0 = 1, DAC_CHANNEL_MASK_CH1 = 2, DAC_CHANNEL_MASK_ALL = 3 } dac_channel_mask_t;
typedef enum { DAC_CHANNEL_MODE_SIMUL, DAC_CHANNEL_MODE_ALTER } dac_continuous_channel_mode_t;
typedef enum { DAC_DIGI_CLK_SRC_DEFAULT, DAC_DIGI_CLK_SRC_APLL } dac_continuous_digi_clk_src_t;
typedef struct {
    dac_channel_mask_t chan_mask;
    uint32_t desc_num;
    size_t buf_size;
    uint32_t freq_hz;
    int8_t offset;
    dac_continuous_digi_clk_src_t clk_src;
    dac_continuous_channel_mode_t chan_mode;
} dac_continuous_config_t;
esp_err_t dac_continuous_new_channels(const dac_continuous_config_t *cfg, dac_continuous_handle_t *out);
esp_err_t dac_continuous_del_channels(dac_continuous_handle_t h);
esp_err_t dac_continuous_enable(dac_continuous_handle_t h);
esp_err_t dac_continuous_disable(dac_continuous_handle_t h);
esp_err_t dac_continuous_write_cyclically(dac_continuous_handle_t h, uint8_t *buf, size_t size, size_t *loaded);
esp_err_t dac_continuous_write(dac_continuous_handle_t h, uint8_t *buf, size_t size, size_t *loaded, int timeout_ms);

// Wi-Fi, ESP-NOW, netif, events, lwIP: idf_net.h

esp_err_t esp_spiffs_format(const char *partition_label);

#endif
