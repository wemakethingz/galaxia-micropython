#ifndef QMC6309_H_
#define QMC6309_H_

#include <stdint.h>

#define QMC6309_X_LSB   0x01
#define QMC6309_X_MSB   0x02
#define QMC6309_Y_LSB   0x03
#define QMC6309_Y_MSB   0x04
#define QMC6309_Z_LSB   0x05
#define QMC6309_Z_MSB   0x06
typedef struct {
  uint8_t out              : 8;
} QMC6309_out_t;

#define QMC6309_STATUS  0x9
typedef struct {
  uint8_t drdy              : 1;
  uint8_t ovfl              : 1;
  uint8_t st_rdy            : 1;
  uint8_t nvm_load_done     : 1;
  uint8_t nvm_ready         : 1;
  uint8_t notused           : 3;
} QMC6309_status_t;

#define QMC6309_CTRL1   0xA
typedef struct {
  uint8_t mode              : 2;
  uint8_t notused           : 1;
  uint8_t osr1              : 2;
  uint8_t osr2              : 3;
} QMC6309_ctrl1_t;

#define QMC6309_CTRL2   0xB
typedef struct {
  uint8_t set_reset              : 2;
  uint8_t rng                    : 2;
  uint8_t odr                    : 3;
  uint8_t soft_reset             : 1;
} QMC6309_ctrl2_t;

#define QMC6309_CTRL3   0xE
typedef struct {
  uint8_t not_used               : 7;
  uint8_t self_test              : 1;
} QMC6309_ctrl3_t;

#define QMC6309_SELFTEST_X   0x13
#define QMC6309_SELFTEST_Y   0x14
#define QMC6309_SELFTEST_Z   0x15

typedef union {
    QMC6309_out_t       out;
    QMC6309_status_t    status;
    QMC6309_ctrl1_t     ctrl1;     
    QMC6309_ctrl2_t     ctrl2;
    QMC6309_ctrl3_t     ctrl3;
    uint8_t             byte;
} QMC6309_reg_t;

typedef int32_t (*i2c_write_ptr)(uint8_t, uint8_t *,
                                    uint16_t);

typedef int32_t (*i2c_read_ptr) (uint8_t, uint8_t *,
                                    uint16_t);

typedef struct {
    i2c_write_ptr  write_reg;
    i2c_read_ptr   read_reg;
} QMC6309_ctx_t;

int32_t QMC6309_read_reg(QMC6309_ctx_t *ctx, uint8_t reg, uint8_t *data, uint16_t len);
int32_t QMC6309_write_reg(QMC6309_ctx_t *ctx, uint8_t reg, uint8_t *data, uint16_t len);


typedef enum {
  QMC6309_MODE_SUSPEND  = 0,
  QMC6309_MODE_NORMAL,
  QMC6309_MODE_SINGLE,
  QMC6309_MODE_CONTINUOUS
} QMC6309_mode_t;

typedef enum {
  QMC6309_OUTPUT_RATE_1HZ  = 0,
  QMC6309_OUTPUT_RATE_10HZ,
  QMC6309_OUTPUT_RATE_50HZ,
  QMC6309_OUTPUT_RATE_100HZ,
  QMC6309_OUTPUT_RATE_200HZ
} QMC6309_output_rate_t;

typedef enum {
  QMC6309_OVERSAMPLE_RATIO_8  = 0,
  QMC6309_OVERSAMPLE_RATIO_4,
  QMC6309_OVERSAMPLE_RATIO_2,
  QMC6309_OVERSAMPLE_RATIO_1
} QMC6309_oversample_ratio_t;

typedef enum {
  QMC6309_DOWNSAMPLING_RATE_1  = 0,
  QMC6309_DOWNSAMPLING_RATE_2,
  QMC6309_DOWNSAMPLING_RATE_4,
  QMC6309_DOWNSAMPLING_RATE_8,
  QMC6309_DOWNSAMPLING_RATE_16,
} QMC6309_downsampling_rate_t;

int32_t QMC6309_set_ctrl1(QMC6309_ctx_t* ctx, QMC6309_ctrl1_t reg);

typedef enum {
  QMC6309_SET_RESET_ON  = 0,
  QMC6309_SET_ONLY_ON,
  QMC6309_SET_RESET_UNUSED,
  QMC6309_SET_RESET_OFF2
} QMC6309_set_reset_mode_t;

typedef enum {
  QMC6309_RANGE_32  = 0,
  QMC6309_RANGE_16,
  QMC6309_RANGE_8,
  QMC6309_RANGE_32bis
} QMC6309_range_t;

int32_t QMC6309_set_ctrl2(QMC6309_ctx_t* ctx, QMC6309_ctrl2_t reg);

int32_t QMC6309_set_ctrl3(QMC6309_ctx_t* ctx, QMC6309_ctrl3_t reg);

int32_t QMC6309_get_status(QMC6309_ctx_t* ctx, QMC6309_status_t* reg);

int32_t QMC6309_get_raw_values(QMC6309_ctx_t* ctx, int16_t *reg);

int32_t QMC6309_reset(QMC6309_ctx_t* ctx);

float QMC6309_get_from_raw_to_mgauss(int16_t raw, QMC6309_range_t range);

int32_t QMC6309_init(QMC6309_ctx_t* ctx, uint8_t range, uint8_t freq);

int32_t QMC6309_get_mgauss(QMC6309_ctx_t* ctx, float *values, uint8_t range);

#endif