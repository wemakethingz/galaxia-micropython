#include "lib/qmc6310u/qmc6310u.h"
#include "esp_err.h"
#include <stdint.h>

int32_t qmc630u_read_reg(qmc630u_ctx_t *ctx, uint8_t reg, uint8_t *data, uint16_t len){
  int32_t ret;
  ret = ctx->read_reg(reg, data, len);
  return ret;
}

int32_t qmc630u_write_reg(qmc630u_ctx_t *ctx, uint8_t reg, uint8_t *data, uint16_t len){
  int32_t ret;
  ret = ctx->write_reg(reg, data, len);
  return ret;
}

int32_t qmc630u_set_ctrl1(qmc630u_ctx_t* ctx, qmc630u_ctrl1_t reg){
    return qmc630u_write_reg(ctx, QMC630U_CTRL1, (uint8_t*)&reg, 1);
}

int32_t qmc630u_set_ctrl2(qmc630u_ctx_t* ctx, qmc630u_ctrl2_t reg){
    return qmc630u_write_reg(ctx, QMC630U_CTRL2, (uint8_t*)&reg, 1);
}


int32_t qmc630u_get_status(qmc630u_ctx_t* ctx, qmc630u_status_t* reg){
    return qmc630u_read_reg(ctx, QMC630U_STATUS, (uint8_t*)reg, 1);
}


int32_t qmc630u_get_raw_values(qmc630u_ctx_t* ctx, int16_t *reg){
    qmc630u_out_t out;
    int32_t err;

    //X
    out.out = 0;
    err = qmc630u_read_reg(ctx, QMC630U_X_LSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[0] = out.out;
    err = qmc630u_read_reg(ctx, QMC630U_X_MSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[0] = out.out << 8 | reg[0];

    //Y
    out.out = 0;
    err = qmc630u_read_reg(ctx, QMC630U_Y_LSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[1] = out.out;
    err = qmc630u_read_reg(ctx, QMC630U_Y_MSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[1] = out.out << 8 | reg[1];

    //Z
    out.out = 0;
    err = qmc630u_read_reg(ctx, QMC630U_Z_LSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[2] = out.out;
    err = qmc630u_read_reg(ctx, QMC630U_Z_MSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[2] = out.out << 8 | reg[2];

    return 0;
}

int32_t qmc630u_reset(qmc630u_ctx_t* ctx){
    qmc630u_ctrl2_t ctl2, ctl2_reset;
    qmc630u_ctrl1_t ctl1;
    esp_err_t err;
    err = qmc630u_read_reg(ctx, QMC630U_CTRL1, (uint8_t*)&ctl1, 1);
    if( err != ESP_OK){
        return err;
    }
    err = qmc630u_read_reg(ctx, QMC630U_CTRL2, (uint8_t*)&ctl2, 1);
    if( err != ESP_OK){
        return err;
    }
    ctl2_reset.soft_reset = 1;
    err = qmc630u_write_reg(ctx, QMC630U_CTRL2, (uint8_t*)&ctl2_reset, 1);
    if( err != ESP_OK){
        return err;
    }
    err = qmc630u_write_reg(ctx, QMC630U_CTRL2, (uint8_t*)&ctl2, 1);
    if( err != ESP_OK){
        return err;
    }
    err = qmc630u_write_reg(ctx, QMC630U_CTRL1, (uint8_t*)&ctl1, 1);
    if( err != ESP_OK){
        return err;
    }
    return ESP_OK;
}

int32_t qmc630u_init(qmc630u_ctx_t* ctx, uint8_t range, uint8_t freq){

    uint8_t data = 0x2;

    qmc630u_ctrl1_t ctl1;
    qmc630u_ctrl2_t ctl2;

    qmc630u_write_reg(ctx, 0x29, &data, 1);

    ctl2.set_reset = QMC630U_SET_RESET_ON;
    ctl2.rng = range;
    ctl2.self_test = 0;
    ctl2.soft_reset = 0;
    qmc630u_set_ctrl2(ctx, ctl2);
    
    ctl1.mode = QMC630U_MODE_NORMAL;
    switch((int)freq){
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

    qmc630u_set_ctrl1(ctx,  ctl1);

    return 0;
}


float qmc630u_get_from_raw_to_mgauss(int16_t raw, qmc630u_range_t range){
    // return (float)raw/2.5f;
    int32_t out_min, out_max_min;
    int32_t in_min, in_max_min;
    in_min = -32768;
    in_max_min = 65536;
    switch (range)
    {
    case QMC630U_RANGE_30:
        return (float)raw/1.0f;
        // out_min = -30000;
        // out_max_min = 60000;
        // break;
    case QMC630U_RANGE_12:
        return (float)raw/2.5f;
        // out_min = -12000;
        // out_max_min = 24000;
        // break;
    case QMC630U_RANGE_8:
        return (float)raw/3.75f;
        // out_min = -8000;
        // out_max_min = 16000;
        // break;
    case QMC630U_RANGE_2:
        return (float)raw/15.0f;
        // out_min = -2000;
        // out_max_min = 4000;
        // break;
    default:
        out_min = 0;
        out_max_min = 0;
        break;
    }
    return (float)(raw - in_min) * (float)(out_max_min) / (float)(in_max_min) + out_min;
}

int32_t qmc630u_get_mgauss(qmc630u_ctx_t* ctx, float *values, uint8_t range){
    int32_t error;
    qmc630u_status_t status;
    uint8_t retry = 0;
    int16_t reg[3];
    do {
        error = qmc630u_get_status(ctx, &status); 
        if(error != ESP_OK){
            return 1;
        }
        retry++;
    } while (!status.drdy && retry <= 3);

    if(status.drdy){
        error = qmc630u_get_raw_values(ctx, reg);
        if(error != ESP_OK){
            return 1;
        }
    }else{
        return 1;
    }
    
    values[0] = qmc630u_get_from_raw_to_mgauss(reg[0], range);
    values[1] = qmc630u_get_from_raw_to_mgauss(reg[1], range);
    values[2] = qmc630u_get_from_raw_to_mgauss(reg[2], range);

    return 0;
}
