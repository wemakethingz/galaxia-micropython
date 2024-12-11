#include "mpconfigboard.h"

#if MICROPY_THINGZ_ACCEL_LIS2DH12

#include "common-thingz/thingz_accel/thingz_accel.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "lib/lis2dh12/lis2dh12.h"

#include "common-thingz/thingz_i2c/thingz_i2c.h"

#if MICROPY_DEBUG_VERBOSE // print debugging info
#define DEBUG_PRINT (1)
#define DEBUG_printf DEBUG_printf
#else // don't print debugging info
#define DEBUG_PRINT (0)
#define DEBUG_printf(...) (void)0
#endif

#define I2C_ACK_VAL  0x0
#define I2C_NACK_VAL 0x1

#define ACCEL_ADDR 0b00110010

#define ACCEL_FREQ 400.0f
#define ACCEL_TIME_BETWEEN_REQUEST 1.0f/ACCEL_FREQ

typedef union {
  int16_t i16bit[3];
  uint8_t u8bit[6];
} axis3bit16_t;

static stmdev_ctx_t ctx_xl;

static const char* TAG = "THGZ_GALAXIA_ACCEL";

static uint8_t bus_id;

static int16_t last_values[3];
static int64_t last_value_timestamp;

static int32_t i2c_write(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len){
    
    reg |= 0x80;
    
    esp_err_t error = common_thingz_i2c_write(ACCEL_ADDR, bus_id, reg, bufp, len);
    
    return error;
}

static int32_t i2c_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len){

    reg |= 0x80;
    esp_err_t error = common_thingz_i2c_read(ACCEL_ADDR, bus_id, reg, bufp, len);

    return error;
}

static void _convert(float* values, int16_t* raw){

    values[0] = lis2dh12_from_fs8_nm_to_mg(raw[1]);
    values[1] = -1.0*lis2dh12_from_fs8_nm_to_mg(raw[0]);
    values[2] = -1.0*lis2dh12_from_fs8_nm_to_mg(raw[2]);
}

static void _used_old_value(float* values){
    _convert(values, last_values);
}

static void _update_old_values(int16_t* values){
    last_values[0] = values[0];
    last_values[1] = values[1];
    last_values[2] = values[2];
}

static void IRAM_ATTR _accel_interupt_cb(void* arg){
    // ESP_LOGI(TAG, "ACCEL INT");
    thingz_accel_new_data_callback(arg);
}

static esp_timer_create_args_t update_timer_args = {
            .callback = &_accel_interupt_cb,
            .name = "accel update",
            .dispatch_method = ESP_TIMER_TASK
};

static esp_timer_handle_t update_timer;

void common_thingz_accel_init(thingz_accel_obj_t* accel, int8_t pinINT, int8_t pinINT2, int8_t i2c_bus_id){
    lis2dh12_reg_t reg;
    gpio_config_t io_conf;
    int i;

    accel->pinINT = pinINT;
    accel->pinINT2 = pinINT2;
    accel->base.type = &thingz_accel_type;

    bus_id = i2c_bus_id;

    ctx_xl.write_reg = i2c_write;
    ctx_xl.read_reg = i2c_read;

    for(i = 0; i < 3; i++){
        last_values[i] = 0;
    }

    last_value_timestamp = 0;

    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = 1ull << pinINT;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 1;

    gpio_reset_pin(pinINT);
    gpio_isr_handler_remove(pinINT);
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(pinINT, _accel_interupt_cb, NULL);

    lis2dh12_operating_mode_set(&(ctx_xl), LIS2DH12_LP_8bit);
    lis2dh12_data_rate_set(&(ctx_xl), LIS2DH12_POWER_DOWN);
    vTaskDelay(9);
    memset(&reg.ctrl_reg3, 0, sizeof(lis2dh12_ctrl_reg3_t));
    reg.ctrl_reg3.i1_zyxda = 0;
    lis2dh12_pin_int1_config_set(&(ctx_xl), &reg.ctrl_reg3);
    

    lis2dh12_operating_mode_set(&(ctx_xl), LIS2DH12_NM_10bit);

    lis2dh12_data_rate_set(&(ctx_xl), LIS2DH12_ODR_50Hz);
    reg.ctrl_reg3.i1_zyxda = 1;
    lis2dh12_pin_int1_config_set(&(ctx_xl), &reg.ctrl_reg3);
    /* Set full scale to 2g. */
    lis2dh12_full_scale_set(&(ctx_xl), LIS2DH12_8g);   

    update_timer_args.arg = accel;
    esp_timer_create(&update_timer_args, &update_timer);
    esp_timer_start_periodic(update_timer, 80000); 
}

void common_thingz_accel_deinit(thingz_accel_obj_t* accel){
    esp_timer_stop(update_timer);
    esp_timer_delete(update_timer);
}

void common_thingz_accel_get_accel(thingz_accel_obj_t* accel, float* values){
    return _used_old_value(values);    
}

void common_thingz_accel_update_values(float *values){
    int16_t data_raw[3];
    uint8_t retry = 0;
    while(retry < 10){
        //DEBUG_printf("#####update values\n");
        if(lis2dh12_acceleration_raw_get(&(ctx_xl), data_raw)){
            retry++;
            
        }else{
            break;
        }
    }
    if(retry < 10){
        _update_old_values(data_raw);
        _convert(values, data_raw);
    }else{
        _used_old_value(values);
    }
    
}


void common_thingz_temp_init(thingz_temp_obj_t* temp, uint8_t i2c_bus_id){
    temp->base.type = &thingz_temp_type;
}

float common_thingz_accel_compass_temp_get_temp(thingz_temp_obj_t* temp){
    return 24.4;
}

#endif