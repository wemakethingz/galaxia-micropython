#include "mpconfigboard.h"

#if MICROPY_THINGZ_MAGNETO_QM6310U

#include "common-thingz/thingz_accel/thingz_accel.h"


#include "driver/gpio.h"
#include "driver/i2c.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "lib/qmc6310u/qmc6310u.h"

#include "common-thingz/thingz_memory/thingz_memory.h"
#include "common-thingz/thingz_i2c/thingz_i2c.h"

#define COMPASS_ADDR 0x1C << 1

#define COMPASS_RANGE QMC630U_RANGE_2
#define COMPASS_FREQ 10.0f
#define COMPASS_TIME_BETWEEN_REQUEST 1.0f/COMPASS_FREQ

static const char* TAG = "THGZ_GALAXIA_COMPASS";

static uint8_t bus_id;

static int16_t last_values[3];
static int64_t last_value_timestamp;
static qmc630u_ctx_t ctx_compass;

static float hardiron_offset[3];
static float softiron_scale[3];

static uint32_t errors_count; 

static int32_t i2c_write(uint8_t reg, uint8_t *bufp, uint16_t len){
    esp_err_t error = common_thingz_i2c_write(COMPASS_ADDR, bus_id, reg, bufp, len);
    if(error == ESP_OK){
        errors_count = 0;
    }else{
        errors_count++;
    }
    return error;
}

static int32_t i2c_read(uint8_t reg, uint8_t *bufp, uint16_t len){
    esp_err_t error = common_thingz_i2c_read(COMPASS_ADDR, bus_id, reg, bufp, len);
    if(error == ESP_OK){
        errors_count = 0;
    }else{
        errors_count++;
    }
    return error;
}

static void _convert(float* values, int16_t* raw, bool calibrate){
    if(calibrate){
        values[0] = qmc630u_get_from_raw_to_mgauss(raw[0], COMPASS_RANGE);
        values[1] = qmc630u_get_from_raw_to_mgauss(raw[1], COMPASS_RANGE);
        values[2] = qmc630u_get_from_raw_to_mgauss(raw[2], COMPASS_RANGE);
    }else{
        values[0] = (qmc630u_get_from_raw_to_mgauss(raw[0], COMPASS_RANGE)-hardiron_offset[0])*softiron_scale[0];//-5;
        values[1] = (qmc630u_get_from_raw_to_mgauss(raw[1], COMPASS_RANGE)-hardiron_offset[1])*softiron_scale[1];//+190;
        values[2] = (qmc630u_get_from_raw_to_mgauss(raw[2], COMPASS_RANGE)-hardiron_offset[2])*softiron_scale[2];
    }
}

static void _used_old_value(float* values, bool calibrate){
    _convert(values, last_values, calibrate);
}

static void _update_old_values(int16_t* values){
    last_values[0] = values[0];
    last_values[1] = values[1];
    last_values[2] = values[2];
}

void common_thingz_compass_init(thingz_compass_obj_t* compass, int8_t pinDRDY, uint8_t i2c_bus_id){   
    qmc630u_ctrl1_t ctl1;
    qmc630u_ctrl2_t ctl2;
    nvs_handle* nvsHandle;
    float tmp;

    int i;

    compass->pinDRDY = pinDRDY;
    compass->base.type = &thingz_compass_type;

    bus_id = i2c_bus_id;

    nvsHandle = thingz_memory_get_handle();
    if(nvs_get_u32(*nvsHandle, "compass_hard_x", (uint32_t*)&tmp) == ESP_OK){
        hardiron_offset[0] = tmp;
    }else{
        hardiron_offset[0] = 0;
    }
    if(nvs_get_u32(*nvsHandle, "compass_hard_y", (uint32_t*)&tmp) == ESP_OK){
        hardiron_offset[1] = tmp;
    }else{
        hardiron_offset[1] = 0;
    }
    if(nvs_get_u32(*nvsHandle, "compass_hard_z", (uint32_t*)&tmp) == ESP_OK){
        hardiron_offset[2] = tmp;
    }else{
        hardiron_offset[2] = 0;
    }

    if(nvs_get_u32(*nvsHandle, "compass_soft_x", (uint32_t*)&tmp) == ESP_OK){
        softiron_scale[0] = tmp;
    }else{
        softiron_scale[0] = 1;
    }
    if(nvs_get_u32(*nvsHandle, "compass_soft_y", (uint32_t*)&tmp) == ESP_OK){
        softiron_scale[1] = tmp;
    }else{
        softiron_scale[1] = 1;
    }
    if(nvs_get_u32(*nvsHandle, "compass_soft_z", (uint32_t*)&tmp) == ESP_OK){
        softiron_scale[2] = tmp;
    }else{
        softiron_scale[2] = 1;
    }
    
    for(i = 0; i < 3; i++){
        last_values[i] = 0;
    }

    last_value_timestamp = 0;

    ctx_compass.read_reg = i2c_read;
    ctx_compass.write_reg = i2c_write;

    errors_count = 0;

    uint8_t data = 0x2;
    qmc630u_write_reg(&ctx_compass, 0x29, &data, 1);

    ctl2.set_reset = QMC630U_SET_RESET_ON;
    ctl2.rng = COMPASS_RANGE;
    ctl2.self_test = 0;
    ctl2.soft_reset = 0;
    qmc630u_set_ctrl2(&ctx_compass, ctl2);
    
    ctl1.mode = QMC630U_MODE_NORMAL;
    switch((int)COMPASS_FREQ){
        case 10:
            ctl1.odr = QMC630U_OUTPUT_RATE_10HZ;
        break;
        case 50:
            ctl1.odr = QMC630U_OUTPUT_RATE_50HZ;
        break;
        case 100:
            ctl1.odr = QMC630U_OUTPUT_RATE_100HZ;
        break;
        case 200:
            ctl1.odr = QMC630U_OUTPUT_RATE_200HZ;
        break;
    }
    
    ctl1.osr1 = QMC630U_OVERSAMPLE_RATIO_8;
    ctl1.osr2 = QMC630U_DOWNSAMPLING_RATE_8;

    qmc630u_set_ctrl1(&ctx_compass,  ctl1);
    
}

void common_thingz_compass_get_gauss(thingz_compass_obj_t* compass, float* values, bool calibrate){
    uint8_t retry = 0;
    qmc630u_status_t status;
    int16_t raw[3];
    esp_err_t error;

    status.drdy = 0;

    if(esp_timer_get_time() - last_value_timestamp < (COMPASS_TIME_BETWEEN_REQUEST)*1000000){
        _used_old_value(values, calibrate);
        ESP_LOGE(TAG, "kppkp");
        return;
    }

    do {
        error = qmc630u_get_status(&ctx_compass, &status); 
        if(error != ESP_OK){
            if(errors_count > 5){
                ESP_LOGE(TAG, "reset: %ld", qmc630u_reset(&ctx_compass));
            }
            ESP_LOGE(TAG, "kppkp l");
            _used_old_value(values, calibrate);
            return;
        }
        retry++;
    } while (!status.drdy && retry <= 30);

    if(status.drdy){
        error = qmc630u_get_raw_values(&ctx_compass, raw);
        if(error != ESP_OK){
            if(errors_count > 5){
                ESP_LOGE(TAG, "reset 2: %ld", qmc630u_reset(&ctx_compass));
            }
            ESP_LOGE(TAG, "kppkp j");
            _used_old_value(values, calibrate);
            return;
        }
    }else{
        ESP_LOGE(TAG, "kppkp nn");
        _used_old_value(values, calibrate);
        return;
    }
    last_value_timestamp = esp_timer_get_time();
    _update_old_values(raw);
    ESP_LOGE(TAG, "raw x: %d, y: %d, z: %d", raw[0], raw[1], raw[2]);
    _convert(values, raw, calibrate);
    ESP_LOGE(TAG, "x: %f, y: %f, z: %f", (double)values[0], (double)values[1], (double)values[2]);
}


void common_thingz_compass_calibrate(thingz_compass_obj_t* compass){
    float min[3] = {0.0f};
    float max[3] = {0.0f};
    float values[3] = {0.0f};
    float avg_delta_axis[3] = {0.0f};
    float avg_delta = 0;

    int i;

    int64_t timestamp = esp_timer_get_time();

    while(esp_timer_get_time() - timestamp < 30000000){
        common_thingz_compass_get_gauss(compass, values, true);

        for(i = 0; i < 3; i++){
            if(values[i] < min[i]){
                min[i] = values[i];
            }
            if(values[i] > max[i]){
                max[i] = values[i];
            }
        }
    }
    //End of hardiron calibration
    for(i = 0; i < 3; i++){
        hardiron_offset[i] = (max[i]+min[i])/2;
        max[i] = 0.0f;
        min[i] = 0.0f;
    }

    timestamp = esp_timer_get_time();
    
    while(esp_timer_get_time() - timestamp < 30000000){
        common_thingz_compass_get_gauss(compass, values, true);

        for(i = 0; i < 3; i++){
            if(values[i] < min[i]){
                min[i] = values[i];
            }
            if(values[i] > max[i]){
                max[i] = values[i];
            }
        }
    }

    //End of soft calibration
    for(i = 0; i < 3; i++){
        avg_delta_axis[i] = (max[i] - min[i])/2;
    }

    avg_delta = (avg_delta_axis[0] + avg_delta_axis[1] + avg_delta_axis[2]) / 3;

    for(i = 0; i < 3; i++){
        softiron_scale[i] = (avg_delta / avg_delta_axis[i] );
    }

    
    // nvs_handle* nvsHandle;
    // nvsHandle = thingz_memory_get_handle();
    
    // nvs_set_u32(*nvsHandle, "compass_hard_x", *(uint32_t*)&hardiron_offset[0]);
    // nvs_set_u32(*nvsHandle, "compass_hard_y", *(uint32_t*)&hardiron_offset[1]);
    // nvs_set_u32(*nvsHandle, "compass_hard_z", *(uint32_t*)&hardiron_offset[2]);

    // nvs_set_u32(*nvsHandle, "compass_soft_x", *(uint32_t*)&softiron_scale[0]);
    // nvs_set_u32(*nvsHandle, "compass_soft_x", *(uint32_t*)&softiron_scale[1]);
    // nvs_set_u32(*nvsHandle, "compass_soft_x", *(uint32_t*)&softiron_scale[2]);

}

#endif