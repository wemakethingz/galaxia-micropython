#ifndef COMMON_THINGZ_ACCEL_H_
#define COMMON_THINGZ_ACCEL_H_

#include "py/obj.h"

#define TGZ_ACCELEROMETER_REST_TOLERANCE               200
#define TGZ_ACCELEROMETER_TILT_TOLERANCE               200
#define TGZ_ACCELEROMETER_FREEFALL_TOLERANCE           400
#define TGZ_ACCELEROMETER_SHAKE_TOLERANCE              1000
#define TGZ_ACCELEROMETER_3G_TOLERANCE                 3072
#define TGZ_ACCELEROMETER_6G_TOLERANCE                 6144
#define TGZ_ACCELEROMETER_8G_TOLERANCE                 8192
#define TGZ_ACCELEROMETER_GESTURE_DAMPING              2
#define TGZ_ACCELEROMETER_SHAKE_DAMPING                10

#define TGZ_ACCELEROMETER_REST_THRESHOLD               (TGZ_ACCELEROMETER_REST_TOLERANCE * TGZ_ACCELEROMETER_REST_TOLERANCE)
#define TGZ_ACCELEROMETER_FREEFALL_THRESHOLD           (TGZ_ACCELEROMETER_FREEFALL_TOLERANCE * TGZ_ACCELEROMETER_FREEFALL_TOLERANCE)
#define TGZ_ACCELEROMETER_3G_THRESHOLD                 (TGZ_ACCELEROMETER_3G_TOLERANCE * TGZ_ACCELEROMETER_3G_TOLERANCE)
#define TGZ_ACCELEROMETER_6G_THRESHOLD                 (TGZ_ACCELEROMETER_6G_TOLERANCE * TGZ_ACCELEROMETER_6G_TOLERANCE)
#define TGZ_ACCELEROMETER_8G_THRESHOLD                 (TGZ_ACCELEROMETER_8G_TOLERANCE * TGZ_ACCELEROMETER_8G_TOLERANCE)
#define TGZ_ACCELEROMETER_SHAKE_COUNT_THRESHOLD        4

extern const mp_obj_type_t thingz_accel_type;
extern const mp_obj_type_t thingz_compass_type;
extern const mp_obj_type_t thingz_temp_type;

typedef enum {
    TGZ_ACCEL_GESTURE_UP =0,
    TGZ_ACCEL_GESTURE_DOWN,
    TGZ_ACCEL_GESTURE_LEFT,
    TGZ_ACCEL_GESTURE_RIGHT,
    TGZ_ACCEL_GESTURE_FACE_UP,
    TGZ_ACCEL_GESTURE_FACE_DOWM,
    TGZ_ACCEL_GESTURE_FREEFALL,
    TGZ_ACCEL_GESTURE_3G,
    TGZ_ACCEL_GESTURE_6G,
    TGZ_ACCEL_GESTURE_8G,
    TGZ_ACCEL_GESTURE_SHAKE,
    TGZ_ACCEL_GESTURE_NONE
} thingz_accel_gesture;

typedef struct {
	mp_obj_base_t base;
    int8_t pinINT;
    int8_t pinINT2;
    mp_obj_t gesture_callback[TGZ_ACCEL_GESTURE_NONE];
    // background_callback_t cb;
} thingz_accel_obj_t;

typedef struct {
	mp_obj_base_t base;
    int8_t pinDRDY;
} thingz_compass_obj_t;


typedef struct {
	mp_obj_base_t base;
} thingz_temp_obj_t;

typedef struct{
    bool x;
    bool y;
    bool z;
    bool shaken;
    uint8_t timer;
    uint8_t count;
} thingz_accel_shake_t;

typedef struct {
    double x, y, z;
} thingz_compass_data_t;

void thingz_accel_compass_temp_init(thingz_accel_obj_t* accel, thingz_compass_obj_t* compass, thingz_temp_obj_t* temp, int8_t pinINT, int8_t pinINT2, int8_t pinDRDY);
void thingz_accel_compass_temp_deinit(thingz_accel_obj_t* accel, thingz_compass_obj_t* compass, thingz_temp_obj_t* temp);

void thingz_accel_new_data_callback(void *args);

void thingz_accel_init_gesture(void);

void common_thingz_accel_init(thingz_accel_obj_t* accel, int8_t pinINT, int8_t pinINT2, int8_t i2c_bus_id);
void common_thingz_temp_init(thingz_temp_obj_t* temp, uint8_t i2c_bus_id);
void common_thingz_compass_init(thingz_compass_obj_t* compass, int8_t pinDRDY, uint8_t i2c_bus_id);

void common_thingz_accel_deinit(thingz_accel_obj_t* accel);

void common_thingz_accel_get_accel(thingz_accel_obj_t* accel, float* values);
void common_thingz_compass_get_gauss(thingz_compass_obj_t* compass, uint8_t raw, thingz_compass_data_t* values);
void common_thingz_compass_calibrate(thingz_compass_obj_t* compass, int duration, int sample);

float common_thingz_accel_compass_temp_get_temp(thingz_temp_obj_t* temp);

void common_thingz_accel_update_values(float *values);

#endif