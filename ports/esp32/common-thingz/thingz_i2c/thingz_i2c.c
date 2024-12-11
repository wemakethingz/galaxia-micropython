#include "thingz_i2c.h"

#include "driver/i2c.h"

// #include "peripherals/i2c.h"

#include "esp_log.h"

#include "semphr.h"

#include "mpconfigboard.h"

#include "py/mpprint.h"

#define I2C_ACK_VAL  0x0
#define I2C_NACK_VAL 0x1

static const char* TAG = "TGZ_I2C";

static SemaphoreHandle_t i2c_semaphore = NULL;
static StaticSemaphore_t i2c_semaphore_buf;
static int8_t i2c_scl, i2c_sda;
static uint32_t i2c_freq;
static int i2c_bus;

void common_thingz_i2c_init (int bus, int8_t scl, int8_t sda, uint32_t freq, bool enable_pullups)
{

    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.scl_io_num = scl;
    conf.sda_pullup_en = enable_pullups;
    conf.scl_pullup_en = enable_pullups;
    conf.master.clk_speed = freq;
    conf.clk_flags = 0;

    i2c_sda = sda;
    i2c_scl = scl;
    i2c_freq = freq;
    i2c_bus = bus;

    if(i2c_param_config(bus, &conf) != ESP_OK){
        ESP_LOGE(TAG, "THGZ i2c init failed\n");
    }

    if(i2c_driver_install(bus, I2C_MODE_MASTER, 0, 0, ESP_INTR_FLAG_IRAM) != ESP_OK){
        ESP_LOGE(TAG, "THGZ i2c driver install failed\n");
    }

    if(i2c_semaphore == NULL){
        i2c_semaphore = xSemaphoreCreateBinaryStatic(&i2c_semaphore_buf);
        xSemaphoreGive(i2c_semaphore);
    }

    // i2c_set_timeout(bus, 10000);
    ESP_LOGI(TAG, "i2c init\n");
}

int32_t common_thingz_i2c_write(uint8_t addr, uint8_t bus_id, uint8_t reg, uint8_t *bufp, uint16_t len){
    esp_err_t error;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr | I2C_MASTER_WRITE, true);
    
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write(cmd, bufp, len, true);
    i2c_master_stop(cmd);
    
    if( xSemaphoreTake( i2c_semaphore, pdMS_TO_TICKS(150)) == pdTRUE ){
        error = i2c_master_cmd_begin(bus_id, cmd,  pdMS_TO_TICKS(100));
        xSemaphoreGive( i2c_semaphore );
    }else{
        error = -1;
    }
    i2c_cmd_link_delete(cmd);
    return error;
}

int32_t common_thingz_i2c_read(uint8_t addr, uint8_t bus_id, uint8_t reg, uint8_t *bufp, uint16_t len){
    esp_err_t error;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( addr  ) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( addr ) | I2C_MASTER_READ, true);
    if (len > 1) i2c_master_read(cmd, bufp, len-1, I2C_ACK_VAL);
    i2c_master_read_byte(cmd, bufp + len-1, I2C_NACK_VAL);
    i2c_master_stop(cmd);
    
    if( xSemaphoreTake( i2c_semaphore, pdMS_TO_TICKS(150)) == pdTRUE ){
        error = i2c_master_cmd_begin(bus_id, cmd,  pdMS_TO_TICKS(100));
        if(error != ESP_OK){
            // mp_printf(MP_PYTHON_PRINTER, "ERREUR: %d\n", error);
            // if(error == -1){
            //     common_thingz_i2c_init(i2c_bus, i2c_scl, i2c_sda, i2c_freq, MICROPY_THINGZ_I2C_USE_PULLUPS);
            // }
        }
        xSemaphoreGive( i2c_semaphore );
    }else{
        // mp_printf(MP_PYTHON_PRINTER, "FAILED TAKING SEMAPHORE\n");
        error = -1;
    }
    i2c_cmd_link_delete(cmd);

    
    return error;
}