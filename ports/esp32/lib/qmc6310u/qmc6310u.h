#ifndef QMC630U_H_
#define QMC630U_H_

#include <stdint.h>

#define QMC630U_X_LSB   0x01
#define QMC630U_X_MSB   0x02
#define QMC630U_Y_LSB   0x03
#define QMC630U_Y_MSB   0x04
#define QMC630U_Z_LSB   0x05
#define QMC630U_Z_MSB   0x06
typedef struct {
  uint8_t out              : 8;
} qmc630u_out_t;

#define QMC630U_STATUS  0x9
typedef struct {
  uint8_t drdy              : 1;
  uint8_t ovfl              : 1;
  uint8_t notused           : 6;
} qmc630u_status_t;

#define QMC630U_CTRL1   0xA
typedef struct {
  uint8_t mode              : 2;
  uint8_t odr               : 2;
  uint8_t osr1              : 2;
  uint8_t osr2              : 2;
} qmc630u_ctrl1_t;

#define QMC630U_CTRL2   0xB
typedef struct {
  uint8_t set_reset              : 2;
  uint8_t rng                    : 2;
  uint8_t notused                : 2;
  uint8_t self_test              : 1;
  uint8_t soft_reset             : 1;
} qmc630u_ctrl2_t;

typedef union {
    qmc630u_out_t       out;
    qmc630u_status_t    status;
    qmc630u_ctrl1_t     ctrl1;     
    qmc630u_ctrl2_t     ctrl2;
    uint8_t             byte;
} qmc630u_reg_t;

typedef int32_t (*i2c_write_ptr)(uint8_t, uint8_t *,
                                    uint16_t);

typedef int32_t (*i2c_read_ptr) (uint8_t, uint8_t *,
                                    uint16_t);

typedef struct {
    i2c_write_ptr  write_reg;
    i2c_read_ptr   read_reg;
} qmc630u_ctx_t;

int32_t qmc630u_read_reg(qmc630u_ctx_t *ctx, uint8_t reg, uint8_t *data, uint16_t len);
int32_t qmc630u_write_reg(qmc630u_ctx_t *ctx, uint8_t reg, uint8_t *data, uint16_t len);


typedef enum {
  QMC630U_MODE_SUSPEND  = 0,
  QMC630U_MODE_NORMAL,
  QMC630U_MODE_SINGLE,
  QMC630U_MODE_CONTINUOUS
} qmc630u_mode_t;

typedef enum {
  QMC630U_OUTPUT_RATE_10HZ  = 0,
  QMC630U_OUTPUT_RATE_50HZ,
  QMC630U_OUTPUT_RATE_100HZ,
  QMC630U_OUTPUT_RATE_200HZ
} qmc630u_output_rate_t;

typedef enum {
  QMC630U_OVERSAMPLE_RATIO_8  = 0,
  QMC630U_OVERSAMPLE_RATIO_4,
  QMC630U_OVERSAMPLE_RATIO_2,
  QMC630U_OVERSAMPLE_RATIO_1
} qmc630u_oversample_ratio_t;

typedef enum {
  QMC630U_DOWNSAMPLING_RATE_1  = 0,
  QMC630U_DOWNSAMPLING_RATE_2,
  QMC630U_DOWNSAMPLING_RATE_4,
  QMC630U_DOWNSAMPLING_RATE_8
} qmc630u_downsampling_rate_t;

int32_t qmc630u_set_ctrl1(qmc630u_ctx_t* ctx, qmc630u_ctrl1_t reg);

typedef enum {
  QMC630U_SET_RESET_ON  = 0,
  QMC630U_SET_ONLY_ON,
  QMC630U_SET_RESET_OFF,
  QMC630U_SET_RESET_OFF2
} qmc630u_set_reset_mode_t;

typedef enum {
  QMC630U_RANGE_30  = 0,
  QMC630U_RANGE_12,
  QMC630U_RANGE_8,
  QMC630U_RANGE_2
} qmc630u_range_t;

int32_t qmc630u_set_ctrl2(qmc630u_ctx_t* ctx, qmc630u_ctrl2_t reg);


int32_t qmc630u_get_status(qmc630u_ctx_t* ctx, qmc630u_status_t* reg);


int32_t qmc630u_get_raw_values(qmc630u_ctx_t* ctx, int16_t *reg);

int32_t qmc630u_reset(qmc630u_ctx_t* ctx);

float qmc630u_get_from_raw_to_mgauss(int16_t raw, qmc630u_range_t range);

#endif