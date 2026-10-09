// What ports/esp32/machine_*.c, modmachine.c and adc.c use of the ESP-IDF 5.4 API
// for the ESP32-S2, for the WebAssembly build: GPIO (matrix, RTC IO, registers),
// general purpose timers (timer_ll), sleep, power management, task watchdog, UART,
// LEDC, ADC and its calibration, DAC one-shot, touch sensor, RMT, SPI master.
// Values that reach Python are ESP-IDF's. The behaviour is in machine_hw.c.
#ifndef GALAXIA_IDF_MACHINE_H
#define GALAXIA_IDF_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ---- SoC (soc_caps.h, soc.h of the ESP32-S2) ----
#define SOC_GPIO_PIN_COUNT              47
#define GPIO_PIN_COUNT                  SOC_GPIO_PIN_COUNT
#define SOC_ADC_PERIPH_NUM              (2)
#define SOC_ADC_ATTEN_NUM               (4)
#define SOC_ADC_RTC_MAX_BITWIDTH        (13)
#define SOC_DAC_SUPPORTED               1
#define SOC_I2S_SUPPORTED               1
#define SOC_RMT_SUPPORTED               1
#define SOC_LEDC_SUPPORT_APB_CLOCK      (1)
#define SOC_LEDC_SUPPORT_REF_TICK       (1)
#define SOC_LEDC_SUPPORT_XTAL_CLOCK     (1)
#define SOC_LEDC_TIMER_BIT_WIDTH        (14)
#define SOC_SPI_PERIPH_NUM              3
#define SOC_TIMER_GROUPS                (2)
#define SOC_TIMER_GROUP_TIMERS_PER_GROUP (2)
#define SOC_UART_NUM                    (2)
#define SOC_UART_HP_NUM                 (2)
#define SOC_UART_FIFO_LEN               (128)
#define SOC_TOUCH_SENSOR_NUM            (15)
#define APB_CLK_FREQ                    (80 * 1000000)
#define REF_CLK_FREQ                    (1000000)
#define XTAL_CLK_FREQ                   (40 * 1000000)
#define SPI2_IOMUX_PIN_NUM_CLK          36
#define SPI2_IOMUX_PIN_NUM_MOSI         35
#define SPI2_IOMUX_PIN_NUM_MISO         37
#define SIG_GPIO_OUT_IDX                256
#define LEDC_LS_SIG_OUT0_IDX            73
#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif
#define ESP_TASK_PRIO_MIN               (0)
#define ESP_TASK_PRIO_MAX               (25)
#define ESP_TASKD_EVENT_PRIO            (ESP_TASK_PRIO_MAX - 5)
typedef int intr_handle_t_;
typedef struct galaxia_intr *intr_handle_t;
typedef void (*intr_handler_t_)(void *arg);
esp_err_t esp_intr_alloc(int source, int flags, void (*handler)(void *), void *arg, intr_handle_t *ret_handle);
esp_err_t esp_intr_free(intr_handle_t handle);

// ---- GPIO ----
enum {
    GPIO_NUM_0, GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7,
    GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_15,
    GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_20, GPIO_NUM_21,
    GPIO_NUM_26 = 26, GPIO_NUM_27, GPIO_NUM_28, GPIO_NUM_29, GPIO_NUM_30, GPIO_NUM_31, GPIO_NUM_32,
    GPIO_NUM_33, GPIO_NUM_34, GPIO_NUM_35, GPIO_NUM_36, GPIO_NUM_37, GPIO_NUM_38, GPIO_NUM_39, GPIO_NUM_40,
    GPIO_NUM_41, GPIO_NUM_42, GPIO_NUM_43, GPIO_NUM_44, GPIO_NUM_45, GPIO_NUM_46, GPIO_NUM_MAX,
};
#define GPIO_MODE_DEF_INPUT  (1 << 0)
#define GPIO_MODE_DEF_OUTPUT (1 << 1)
#define GPIO_MODE_DEF_OD     (1 << 2)
// Valid pads of the ESP32-S2: 0-21, 26-46; 46 is input only
#define GPIO_IS_VALID_GPIO(n) (((n) >= 0 && (n) <= 21) || ((n) >= 26 && (n) <= 46))
#define GPIO_IS_VALID_OUTPUT_GPIO(n) (GPIO_IS_VALID_GPIO(n) && (n) != 46)
esp_err_t gpio_pullup_en(gpio_num_t pin);
esp_err_t gpio_pullup_dis(gpio_num_t pin);
esp_err_t gpio_pulldown_en(gpio_num_t pin);
esp_err_t gpio_pulldown_dis(gpio_num_t pin);
esp_err_t gpio_hold_en(gpio_num_t pin);
esp_err_t gpio_hold_dis(gpio_num_t pin);
void esp_rom_gpio_pad_select_gpio(uint32_t pin);
void esp_rom_gpio_connect_out_signal(uint32_t pin, uint32_t signal, bool out_inv, bool oen_inv);
bool rtc_gpio_is_valid_gpio(gpio_num_t pin);
esp_err_t rtc_gpio_deinit(gpio_num_t pin);
// Output registers (bit-banged bitstream)
#define GPIO_OUT_W1TS_REG   0x3f404008
#define GPIO_OUT_W1TC_REG   0x3f40400c
#define GPIO_OUT1_W1TS_REG  0x3f404014
#define GPIO_OUT1_W1TC_REG  0x3f404018
void galaxia_gpio_reg_write(uint32_t reg, uint32_t value);
// The GPIO registers machine_pin.c reads (interrupt type of a pin)
typedef struct {
    struct { uint32_t int_type; } pin[SOC_GPIO_PIN_COUNT];
} gpio_dev_t;
gpio_dev_t *galaxia_gpio_dev(void);
#define GPIO (*galaxia_gpio_dev())
#define GPIO_REG_WRITE(reg, value) galaxia_gpio_reg_write(reg, value)
#define REG_WRITE(reg, value) ((void)(reg), (void)(value))
#define RTC_CNTL_OPTION1_REG 0
#define RTC_CNTL_FORCE_DOWNLOAD_BOOT 1

// ---- CPU, reset, sleep, power management ----
uint32_t esp_rom_get_cpu_ticks_per_us(void);
void esp_rom_delay_us(uint32_t us);
esp_err_t esp_efuse_mac_get_default(uint8_t *mac);
typedef enum {
    ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC, ESP_RST_INT_WDT,
    ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT, ESP_RST_SDIO,
} esp_reset_reason_t;
esp_reset_reason_t esp_reset_reason(void);
typedef enum {
    ESP_SLEEP_WAKEUP_UNDEFINED, ESP_SLEEP_WAKEUP_ALL, ESP_SLEEP_WAKEUP_EXT0, ESP_SLEEP_WAKEUP_EXT1,
    ESP_SLEEP_WAKEUP_TIMER, ESP_SLEEP_WAKEUP_TOUCHPAD, ESP_SLEEP_WAKEUP_ULP, ESP_SLEEP_WAKEUP_GPIO,
    ESP_SLEEP_WAKEUP_UART,
} esp_sleep_source_t;
typedef esp_sleep_source_t esp_sleep_wakeup_cause_t;
typedef enum { ESP_EXT1_WAKEUP_ALL_LOW = 0, ESP_EXT1_WAKEUP_ANY_HIGH = 1 } esp_sleep_ext1_wakeup_mode_t;
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t source);
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us);
esp_err_t esp_sleep_enable_ext0_wakeup(gpio_num_t gpio_num, int level);
esp_err_t esp_sleep_enable_ext1_wakeup(uint64_t io_mask, esp_sleep_ext1_wakeup_mode_t level_mode);
esp_err_t esp_sleep_enable_touchpad_wakeup(void);
esp_err_t esp_sleep_enable_ulp_wakeup(void);
esp_err_t esp_light_sleep_start(void);
void esp_deep_sleep_start(void) __attribute__((noreturn));
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void);
typedef struct {
    int max_freq_mhz;
    int min_freq_mhz;
    bool light_sleep_enable;
} esp_pm_config_t;
esp_err_t esp_pm_configure(const void *config);

// ---- task watchdog ----
typedef struct {
    uint32_t timeout_ms;
    uint32_t idle_core_mask;
    bool trigger_panic;
} esp_task_wdt_config_t;
typedef struct galaxia_wdt_user *esp_task_wdt_user_handle_t;
esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config);
esp_err_t esp_task_wdt_add_user(const char *user_name, esp_task_wdt_user_handle_t *user_handle_ret);
esp_err_t esp_task_wdt_reset_user(esp_task_wdt_user_handle_t user_handle);

// ---- FreeRTOS tasks (UART events) ----
typedef void (*TaskFunction_t)(void *);
typedef uint8_t StackType_t;      // stack depths are in bytes in ESP-IDF
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack_depth, void *arg,
    UBaseType_t priority, TaskHandle_t *created_task, BaseType_t core_id);
void vTaskDelete(TaskHandle_t task);
#define tskNO_AFFINITY 0x7FFFFFFF
#define xTaskCreate(fn, name, depth, arg, priority, handle) \
    xTaskCreatePinnedToCore(fn, name, depth, arg, priority, handle, tskNO_AFFINITY)
#define MP_TASK_COREID (0)        // CONFIG_FREERTOS_UNICORE: the ESP32-S2 has one core
#define taskYIELD() ((void)0)

// ---- general purpose timers (hal/timer_ll.h) ----
typedef struct galaxia_timer_group timg_dev_t;
typedef struct {
    timg_dev_t *dev;
    uint32_t timer_id;
} timer_hal_context_t;
typedef enum { GPTIMER_CLK_SRC_APB = 0, GPTIMER_CLK_SRC_XTAL, GPTIMER_CLK_SRC_DEFAULT = GPTIMER_CLK_SRC_APB } gptimer_clock_source_t;
typedef enum { GPTIMER_COUNT_DOWN, GPTIMER_COUNT_UP } gptimer_count_direction_t;
#define TIMER_LL_EVENT_ALARM(timer) (1 << (timer))
typedef struct {
    struct { int timer_irq_id[2]; } groups[2];
} timer_group_signal_conn_t;
extern const timer_group_signal_conn_t timer_group_periph_signals;
void timer_hal_init(timer_hal_context_t *hal, uint32_t group_num, uint32_t timer_num);
void timer_hal_set_counter_value(timer_hal_context_t *hal, uint64_t load_val);
void timer_ll_enable_counter(timg_dev_t *hw, uint32_t timer_num, bool en);
void timer_ll_set_clock_source(timg_dev_t *hw, uint32_t timer_num, gptimer_clock_source_t clk_src);
void timer_ll_set_clock_prescale(timg_dev_t *hw, uint32_t timer_num, uint32_t divider);
void timer_ll_set_count_direction(timg_dev_t *hw, uint32_t timer_num, gptimer_count_direction_t direction);
void timer_ll_enable_intr(timg_dev_t *hw, uint32_t mask, bool en);
void timer_ll_clear_intr_status(timg_dev_t *hw, uint32_t mask);
uint32_t timer_ll_get_intr_status(timg_dev_t *hw);
void timer_ll_set_alarm_value(timg_dev_t *hw, uint32_t timer_num, uint64_t alarm_value);
void timer_ll_enable_alarm(timg_dev_t *hw, uint32_t timer_num, bool en);
void timer_ll_set_reload_value(timg_dev_t *hw, uint32_t timer_num, uint64_t load_val);
void timer_ll_enable_auto_reload(timg_dev_t *hw, uint32_t timer_num, bool en);
uint64_t timer_ll_get_counter_value(timg_dev_t *hw, uint32_t timer_num);
void timer_ll_trigger_soft_capture(timg_dev_t *hw, uint32_t timer_num);

// ---- I2C (the driver is in peripherals.c) ----
#define I2C_NUM_0 0
#define I2C_NUM_1 1
#define I2C_NUM_MAX 2
#define I2C_LL_MAX_TIMEOUT 0x3fffff
#ifndef I2C_MASTER_LAST_NACK
#define I2C_MASTER_LAST_NACK 2
#endif
esp_err_t i2c_get_period(int i2c_num, int *high_period, int *low_period);

// ---- SPI master (the bus is in idf_shim.c) ----
#define SPI_DEVICE_TXBIT_LSBFIRST (1 << 0)
#define SPI_DEVICE_RXBIT_LSBFIRST (1 << 1)
esp_err_t spi_bus_remove_device(spi_device_handle_t handle);
esp_err_t spi_bus_free(spi_host_device_t host);
int spi_get_actual_clock(int fapb, int hz, int duty_cycle);
esp_err_t spi_device_acquire_bus(spi_device_handle_t device, TickType_t wait);
void spi_device_release_bus(spi_device_handle_t dev);

// ---- UART ----
typedef int uart_port_t;
#define UART_NUM_0 0
#define UART_NUM_1 1
#define UART_NUM_MAX 2
#define UART_PIN_NO_CHANGE (-1)
#define UART_FIFO_LEN SOC_UART_FIFO_LEN
#define UART_HW_FIFO_LEN(uart_num) SOC_UART_FIFO_LEN
typedef enum { UART_DATA_5_BITS = 0x0, UART_DATA_6_BITS = 0x1, UART_DATA_7_BITS = 0x2, UART_DATA_8_BITS = 0x3, UART_DATA_BITS_MAX = 0x4 } uart_word_length_t;
typedef enum { UART_STOP_BITS_1 = 0x1, UART_STOP_BITS_1_5 = 0x2, UART_STOP_BITS_2 = 0x3, UART_STOP_BITS_MAX = 0x4 } uart_stop_bits_t;
typedef enum { UART_PARITY_DISABLE = 0x0, UART_PARITY_EVEN = 0x2, UART_PARITY_ODD = 0x3 } uart_parity_t;
typedef enum { UART_HW_FLOWCTRL_DISABLE = 0x0, UART_HW_FLOWCTRL_RTS = 0x1, UART_HW_FLOWCTRL_CTS = 0x2, UART_HW_FLOWCTRL_CTS_RTS = 0x3, UART_HW_FLOWCTRL_MAX = 0x4 } uart_hw_flowcontrol_t;
typedef enum {
    UART_SIGNAL_INV_DISABLE = 0, UART_SIGNAL_IRDA_TX_INV = (0x1 << 0), UART_SIGNAL_IRDA_RX_INV = (0x1 << 1),
    UART_SIGNAL_RXD_INV = (0x1 << 2), UART_SIGNAL_CTS_INV = (0x1 << 3), UART_SIGNAL_DSR_INV = (0x1 << 4),
    UART_SIGNAL_TXD_INV = (0x1 << 5), UART_SIGNAL_RTS_INV = (0x1 << 6), UART_SIGNAL_DTR_INV = (0x1 << 7),
} uart_signal_inv_t;
typedef enum { UART_SCLK_APB = 0, UART_SCLK_REF_TICK, UART_SCLK_DEFAULT = UART_SCLK_APB } uart_sclk_t;
typedef struct {
    int baud_rate;
    uart_word_length_t data_bits;
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
    uart_hw_flowcontrol_t flow_ctrl;
    uint8_t rx_flow_ctrl_thresh;
    uart_sclk_t source_clk;
} uart_config_t;
typedef enum {
    UART_DATA, UART_BREAK, UART_BUFFER_FULL, UART_FIFO_OVF, UART_FRAME_ERR, UART_PARITY_ERR,
    UART_DATA_BREAK, UART_PATTERN_DET, UART_EVENT_MAX,
} uart_event_type_t;
typedef struct {
    uart_event_type_t type;
    size_t size;
    bool timeout_flag;
} uart_event_t;
esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int queue_size, QueueHandle_t *uart_queue, int intr_alloc_flags);
esp_err_t uart_driver_delete(uart_port_t uart_num);
bool uart_is_driver_installed(uart_port_t uart_num);
esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t *uart_config);
esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);
esp_err_t uart_set_baudrate(uart_port_t uart_num, uint32_t baudrate);
esp_err_t uart_get_baudrate(uart_port_t uart_num, uint32_t *baudrate);
esp_err_t uart_set_word_length(uart_port_t uart_num, uart_word_length_t data_bit);
esp_err_t uart_get_word_length(uart_port_t uart_num, uart_word_length_t *data_bit);
esp_err_t uart_set_parity(uart_port_t uart_num, uart_parity_t parity_mode);
esp_err_t uart_get_parity(uart_port_t uart_num, uart_parity_t *parity_mode);
esp_err_t uart_set_stop_bits(uart_port_t uart_num, uart_stop_bits_t stop_bits);
esp_err_t uart_get_stop_bits(uart_port_t uart_num, uart_stop_bits_t *stop_bits);
esp_err_t uart_set_rx_full_threshold(uart_port_t uart_num, int threshold);
esp_err_t uart_set_rx_timeout(uart_port_t uart_num, const uint8_t tout_thresh);
esp_err_t uart_set_line_inverse(uart_port_t uart_num, uint32_t inverse_mask);
esp_err_t uart_set_hw_flow_ctrl(uart_port_t uart_num, uart_hw_flowcontrol_t flow_ctrl, uint8_t rx_thresh);
esp_err_t uart_get_buffered_data_len(uart_port_t uart_num, size_t *size);
esp_err_t uart_wait_tx_done(uart_port_t uart_num, TickType_t ticks_to_wait);
int uart_read_bytes(uart_port_t uart_num, void *buf, uint32_t length, TickType_t ticks_to_wait);
int uart_write_bytes(uart_port_t uart_num, const void *src, size_t size);
esp_err_t uart_flush_input(uart_port_t uart_num);
esp_err_t uart_get_sclk_freq(uart_sclk_t sclk, uint32_t *out_freq_hz);

// ---- LEDC (the channels are in peripherals.c) ----
#define LEDC_CHANNEL_MAX 8
#define LEDC_TIMER_MAX 4
typedef enum { LEDC_TIMER_1_BIT = 1, LEDC_TIMER_2_BIT, LEDC_TIMER_3_BIT, LEDC_TIMER_4_BIT, LEDC_TIMER_5_BIT,
    LEDC_TIMER_6_BIT, LEDC_TIMER_7_BIT, LEDC_TIMER_8_BIT, LEDC_TIMER_9_BIT, LEDC_TIMER_10_BIT, LEDC_TIMER_11_BIT,
    LEDC_TIMER_12_BIT, LEDC_TIMER_13_BIT, LEDC_TIMER_14_BIT, LEDC_TIMER_BIT_MAX, LEDC_TIMER_16_BIT = 16 } ledc_timer_bit_t;
typedef enum { LEDC_AUTO_CLK = 0, LEDC_USE_APB_CLK, LEDC_USE_RC_FAST_CLK, LEDC_USE_REF_TICK, LEDC_USE_XTAL_CLK } ledc_clk_cfg_t;
typedef enum { LEDC_INTR_DISABLE = 0, LEDC_INTR_FADE_END } ledc_intr_type_t;
esp_err_t ledc_set_duty(ledc_mode_t speed_mode, int channel, uint32_t duty);
esp_err_t ledc_update_duty(ledc_mode_t speed_mode, int channel);
uint32_t ledc_get_duty(ledc_mode_t speed_mode, int channel);
esp_err_t ledc_timer_rst(ledc_mode_t speed_mode, ledc_timer_t timer_sel);
esp_err_t ledc_bind_channel_timer(ledc_mode_t speed_mode, int channel, ledc_timer_t timer_sel);

// ---- ADC (legacy driver and calibration) ----
typedef enum { ADC_UNIT_1, ADC_UNIT_2 } adc_unit_t;
typedef enum { ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5,
    ADC_CHANNEL_6, ADC_CHANNEL_7, ADC_CHANNEL_8, ADC_CHANNEL_9 } adc_channel_t;
typedef enum { ADC_BITWIDTH_DEFAULT = 0, ADC_BITWIDTH_13 = 13 } adc_bitwidth_t;
typedef struct galaxia_adc_cali *adc_cali_handle_t;
typedef struct {
    adc_unit_t unit_id;
    int chan;
    int atten;
    int bitwidth;
    uint32_t default_vref;
} adc_cali_line_fitting_config_t;
#define ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED 1
esp_err_t adc_cali_create_scheme_line_fitting(const adc_cali_line_fitting_config_t *config, adc_cali_handle_t *ret_handle);
esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t handle, int raw, int *voltage);
esp_err_t adc1_config_width(adc_bits_width_t width_bit);
esp_err_t adc2_config_channel_atten(int channel, int atten);
esp_err_t adc2_get_raw(int channel, adc_bits_width_t width_bit, int *raw_out);

// ---- DAC one-shot ----
typedef enum { DAC_CHAN_0 = 0, DAC_CHAN_1 = 1 } dac_channel_t;
typedef struct galaxia_dac_oneshot *dac_oneshot_handle_t;
typedef struct { dac_channel_t chan_id; } dac_oneshot_config_t;
esp_err_t dac_oneshot_new_channel(const dac_oneshot_config_t *oneshot_cfg, dac_oneshot_handle_t *ret_handle);
esp_err_t dac_oneshot_del_channel(dac_oneshot_handle_t handle);
esp_err_t dac_oneshot_output_voltage(dac_oneshot_handle_t handle, uint8_t digi_value);

// ---- touch sensor ----
enum { TOUCH_PAD_NUM0, TOUCH_PAD_NUM1, TOUCH_PAD_NUM2, TOUCH_PAD_NUM3, TOUCH_PAD_NUM4, TOUCH_PAD_NUM5,
    TOUCH_PAD_NUM6, TOUCH_PAD_NUM7, TOUCH_PAD_NUM8, TOUCH_PAD_NUM9, TOUCH_PAD_NUM10, TOUCH_PAD_NUM11,
    TOUCH_PAD_NUM12, TOUCH_PAD_NUM13, TOUCH_PAD_NUM14, TOUCH_PAD_MAX };
esp_err_t touch_pad_read_raw_data(touch_pad_t touch_num, uint32_t *raw_data);

// ---- RMT (bitstream) ----
typedef int rmt_channel_t;
#define RMT_CHANNEL_MAX 4
typedef struct {
    union {
        struct {
            uint32_t duration0 : 15;
            uint32_t level0 : 1;
            uint32_t duration1 : 15;
            uint32_t level1 : 1;
        };
        uint32_t val;
    };
} rmt_item32_t;
typedef void (*sample_to_rmt_t)(const void *src, rmt_item32_t *dest, size_t src_size, size_t wanted_num, size_t *translated_size, size_t *item_num);
typedef struct {
    int rmt_mode;
    rmt_channel_t channel;
    int gpio_num;
    uint8_t clk_div;
    uint8_t mem_block_num;
    uint32_t flags;
    struct { bool loop_en; bool carrier_en; bool idle_output_en; int idle_level; } tx_config;
} rmt_config_t;
#define RMT_DEFAULT_CONFIG_TX(gpio, channel_id) { .rmt_mode = 0, .channel = (channel_id), .gpio_num = (gpio), .clk_div = 80, .mem_block_num = 1 }
esp_err_t rmt_config(const rmt_config_t *rmt_param);
esp_err_t rmt_driver_install_core1(uint8_t channel_id);
esp_err_t rmt_driver_uninstall(rmt_channel_t channel);
esp_err_t rmt_get_counter_clock(rmt_channel_t channel, uint32_t *clock_hz);
esp_err_t rmt_translator_init(rmt_channel_t channel, sample_to_rmt_t fn);
esp_err_t rmt_write_sample(rmt_channel_t channel, const uint8_t *src, size_t src_size, bool wait_tx_done);
esp_err_t rmt_wait_tx_done(rmt_channel_t channel, TickType_t wait_time);
extern int8_t esp32_rmt_bitstream_channel_id;

#endif
