#include "lib/qmc6309/qmc6309.h"
#include "esp_err.h"
#include "qmc6309.h"
#include "py/mpprint.h"

int32_t QMC6309_read_reg(QMC6309_ctx_t *ctx, uint8_t reg, uint8_t *data, uint16_t len){
  int32_t ret;
  ret = ctx->read_reg(reg, data, len);
  return ret;
}

int32_t QMC6309_write_reg(QMC6309_ctx_t *ctx, uint8_t reg, uint8_t *data, uint16_t len){
  int32_t ret;
  ret = ctx->write_reg(reg, data, len);
  return ret;
}

int32_t QMC6309_set_ctrl1(QMC6309_ctx_t* ctx, QMC6309_ctrl1_t reg){
    return QMC6309_write_reg(ctx, QMC6309_CTRL1, (uint8_t*)&reg, 1);
}

int32_t QMC6309_set_ctrl2(QMC6309_ctx_t* ctx, QMC6309_ctrl2_t reg){
    return QMC6309_write_reg(ctx, QMC6309_CTRL2, (uint8_t*)&reg, 1);
}

int32_t QMC6309_set_ctrl3(QMC6309_ctx_t* ctx, QMC6309_ctrl3_t reg){
    return QMC6309_write_reg(ctx, QMC6309_CTRL3, (uint8_t*)&reg, 1);
}

int32_t QMC6309_get_status(QMC6309_ctx_t* ctx, QMC6309_status_t* reg){
    return QMC6309_read_reg(ctx, QMC6309_STATUS, (uint8_t*)reg, 1);
}


int32_t QMC6309_get_raw_values(QMC6309_ctx_t* ctx, int16_t *reg){
    QMC6309_out_t out;
    int32_t err;

    //X
    out.out = 0;
    err = QMC6309_read_reg(ctx, QMC6309_X_LSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[0] = out.out;
    err = QMC6309_read_reg(ctx, QMC6309_X_MSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[0] = out.out << 8 | reg[0];

    //Y
    out.out = 0;
    err = QMC6309_read_reg(ctx, QMC6309_Y_LSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[1] = out.out;
    err = QMC6309_read_reg(ctx, QMC6309_Y_MSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[1] = out.out << 8 | reg[1];

    //Z
    out.out = 0;
    err = QMC6309_read_reg(ctx, QMC6309_Z_LSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[2] = out.out;
    err = QMC6309_read_reg(ctx, QMC6309_Z_MSB, (uint8_t*)&out, 1);
    if(err != ESP_OK){
        return err;
    }
    reg[2] = out.out << 8 | reg[2];

    return 0;
}

int32_t QMC6309_reset(QMC6309_ctx_t* ctx){
    QMC6309_ctrl2_t ctl2;
    QMC6309_ctrl1_t ctl1;

    esp_err_t err;
    err = QMC6309_read_reg(ctx, QMC6309_CTRL1, (uint8_t*)&ctl1, 1);
    if( err != ESP_OK){
        return err;
    }
    err = QMC6309_read_reg(ctx, QMC6309_CTRL2, (uint8_t*)&ctl2, 1);
    if( err != ESP_OK){
        return err;
    }
    ctl2.soft_reset = 1;
    err = QMC6309_write_reg(ctx, QMC6309_CTRL2, (uint8_t*)&ctl2, 1);
    if( err != ESP_OK){
        return err;
    }
    ctl2.soft_reset = 0;
    err = QMC6309_write_reg(ctx, QMC6309_CTRL2, (uint8_t*)&ctl2, 1);
    if( err != ESP_OK){
        return err;
    }
    err = QMC6309_write_reg(ctx, QMC6309_CTRL1, (uint8_t*)&ctl1, 1);
    if( err != ESP_OK){
        return err;
    }
    return ESP_OK;
}

int32_t QMC6309_init(QMC6309_ctx_t* ctx, uint8_t range, uint8_t freq){

    QMC6309_ctrl1_t ctl1;
    QMC6309_ctrl2_t ctl2;
    
    ctl2.set_reset = QMC6309_SET_RESET_ON;
    ctl2.rng = range;
    ctl2.soft_reset = 0;

    
    switch((int)freq){
        case 1:
            ctl2.odr = QMC6309_OUTPUT_RATE_1HZ;
        break;
        case 10:
            ctl2.odr = QMC6309_OUTPUT_RATE_10HZ;
        break;
        case 50:
            ctl2.odr = QMC6309_OUTPUT_RATE_50HZ;
        break;
        case 100:
            ctl2.odr = QMC6309_OUTPUT_RATE_100HZ;
        break;
        case 200:
            ctl2.odr = QMC6309_OUTPUT_RATE_200HZ;
        break;
    }

    QMC6309_set_ctrl2(ctx, ctl2);

    ctl1.mode = QMC6309_MODE_NORMAL;
    ctl1.osr1 = QMC6309_OVERSAMPLE_RATIO_8;
    ctl1.osr2 = QMC6309_DOWNSAMPLING_RATE_16;

    QMC6309_set_ctrl1(ctx,  ctl1);

    return 0;
}



float QMC6309_get_from_raw_to_mgauss(int16_t raw, QMC6309_range_t range){
    // return (float)raw/2.5f;
    int32_t out_min, out_max_min;
    int32_t in_min, in_max_min;
    in_min = -32768;
    in_max_min = 65536;
    switch (range)
    {
    case QMC6309_RANGE_32:
        return (float)raw/1.0f;
        // out_min = -30000;
        // out_max_min = 60000;
        // break;
    case QMC6309_RANGE_16:
        return (float)raw/2.0f;
        // out_min = -12000;
        // out_max_min = 24000;
        // break;
    case QMC6309_RANGE_8:
        return (float)raw/4.0f;
        // out_min = -8000;
        // out_max_min = 16000;
        // break;
    default:
        out_min = 0;
        out_max_min = 0;
        break;
    }
    return (float)(raw - in_min) * (float)(out_max_min) / (float)(in_max_min) + out_min;
}

int32_t QMC6309_get_mgauss(QMC6309_ctx_t* ctx, float *values, uint8_t range){
    int32_t error;
    QMC6309_status_t status;
    uint8_t retry = 0;
    int16_t reg[3];
    do {
        error = QMC6309_get_status(ctx, &status); 
        // mp_printf(MP_PYTHON_PRINTER, "STATUS RET: %d\n", status.drdy);
        if(error != ESP_OK){
            return 1;
        }
        retry++;
    } while (!status.drdy && retry <= 3);

    if(status.drdy){
        error = QMC6309_get_raw_values(ctx, reg);
        if(error != ESP_OK){
            return 1;
        }
    }else{
        return 1;
    }
    // mp_printf(MP_PYTHON_PRINTER, "reg 1 %d reg2 %d reg3 %d\n", reg[0], reg[1], reg[2]);
    values[0] = QMC6309_get_from_raw_to_mgauss(reg[0], range);
    values[1] = QMC6309_get_from_raw_to_mgauss(reg[1], range);
    values[2] = QMC6309_get_from_raw_to_mgauss(reg[2], range);

    return 0;
}
