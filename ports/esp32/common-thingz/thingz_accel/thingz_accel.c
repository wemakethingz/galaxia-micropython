#include "thingz_accel.h"

#include <string.h>
#include <math.h>

#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objfun.h"

#include "driver/gpio.h"
#include "driver/i2c.h"

#include "esp_timer.h"
#include "esp_log.h"

#include "common-thingz/thingz_i2c/thingz_i2c.h"

#include "task.h"
#include "queue.h"

#if MICROPY_DEBUG_VERBOSE // print debugging info
#define DEBUG_PRINT (1)
#define DEBUG_printf DEBUG_printf
#else // don't print debugging info
#define DEBUG_PRINT (0)
#define DEBUG_printf(...) (void)0
#endif

#define TGZ_ACCEL_GESTURE_HISTORY_LENGTH 10
#define TGZ_ACCEL_GESTURE_NUMBER_OF_GESTURE 11
#define TGZ_ACCEL_GESTURE_NUMBER_OF_GESTURE_WIHT_NONE 12

static const char* TAG = "THGZ_ACCEL";

static uint8_t thingz_accel_current_history_pos;
static thingz_accel_gesture thingz_accel_gesture_history[TGZ_ACCEL_GESTURE_HISTORY_LENGTH];
static uint32_t thingz_accel_was_gesture;
static thingz_accel_gesture thingz_accel_current_gesture, thingz_accel_last_gesture;
static thingz_accel_shake_t thingz_accel_shake;
static uint8_t thingz_accel_sigma;

static const char* thingz_accel_gesture_to_str[] = {
    "up",
    "down",
    "left",
    "right",
    "face up",
    "face down",
    "freefall",
    "3g",
    "6g",
    "8g",
    "shake",
    "none"
};
static void _thingz_accel_update_gesture(float* values, void *args);

void thingz_accel_init_gesture(void){
    int i;

    thingz_accel_current_history_pos = 0;
    for(i = 0; i < TGZ_ACCEL_GESTURE_HISTORY_LENGTH; i++){
        thingz_accel_gesture_history[i] = TGZ_ACCEL_GESTURE_NONE;
    }

    thingz_accel_current_gesture = TGZ_ACCEL_GESTURE_NONE;
    thingz_accel_last_gesture = TGZ_ACCEL_GESTURE_NONE;

    thingz_accel_shake.shaken = 0;
    thingz_accel_shake.count = 0;
    thingz_accel_shake.timer = 0;
    thingz_accel_shake.x = 0;
    thingz_accel_shake.y = 0;
    thingz_accel_shake.z = 0;

    thingz_accel_sigma = 0;

    thingz_accel_was_gesture = 0;
}

static void _thingz_accel_gesture_history_push(thingz_accel_gesture g){
    int i;
    if(thingz_accel_gesture_history[thingz_accel_current_history_pos] == g || g == TGZ_ACCEL_GESTURE_NONE)
        return;
    if(thingz_accel_gesture_history[thingz_accel_current_history_pos] != TGZ_ACCEL_GESTURE_NONE)
        thingz_accel_current_history_pos++;
    
    if(thingz_accel_current_history_pos == TGZ_ACCEL_GESTURE_HISTORY_LENGTH){
        thingz_accel_current_history_pos--;
        //shift
        for(i = 0; i < thingz_accel_current_history_pos; i++){
            thingz_accel_gesture_history[i] = thingz_accel_gesture_history[i+1];
        }
    }
    thingz_accel_gesture_history[thingz_accel_current_history_pos] = g;
    
}

static float _thingz_accel_instant_accel_squared(float* values) {
    
    return values[0]*values[0] + values[1]*values[1] + values[2]*values[2];
}


static thingz_accel_gesture _thingz_accel_instant_posture(float* values){
    float force = _thingz_accel_instant_accel_squared(values);
    bool shakeDetected = false;

    // Test for shake events.
    // We detect a shake by measuring zero crossings in each axis. In other words, if we see a strong acceleration to the left followed by
    // a string acceleration to the right, then we can infer a shake. Similarly, we can do this for each acxis (left/right, up/down, in/out).

    if ((values[0] < -TGZ_ACCELEROMETER_SHAKE_TOLERANCE && thingz_accel_shake.x) || (values[0] > TGZ_ACCELEROMETER_SHAKE_TOLERANCE && !thingz_accel_shake.x))
    {
        shakeDetected = true;
        thingz_accel_shake.x = !thingz_accel_shake.x;
    }

    if ((values[1] < -TGZ_ACCELEROMETER_SHAKE_TOLERANCE && thingz_accel_shake.y) || (values[1] > TGZ_ACCELEROMETER_SHAKE_TOLERANCE && !thingz_accel_shake.y))
    {
        shakeDetected = true;
        thingz_accel_shake.y = !thingz_accel_shake.y;
    }

    if ((values[2] < -TGZ_ACCELEROMETER_SHAKE_TOLERANCE && thingz_accel_shake.z) || (values[2] > TGZ_ACCELEROMETER_SHAKE_TOLERANCE && !thingz_accel_shake.z))
    {
        shakeDetected = true;
        thingz_accel_shake.z = !thingz_accel_shake.z;
    }

    if (shakeDetected && thingz_accel_shake.count < TGZ_ACCELEROMETER_SHAKE_COUNT_THRESHOLD && ++thingz_accel_shake.count == TGZ_ACCELEROMETER_SHAKE_COUNT_THRESHOLD)
        thingz_accel_shake.shaken = 1;

    if (++thingz_accel_shake.timer >= TGZ_ACCELEROMETER_SHAKE_DAMPING)
    {
        thingz_accel_shake.timer = 0;
        if (thingz_accel_shake.count > 0)
        {
            if(--thingz_accel_shake.count == 0)
                thingz_accel_shake.shaken = 0;
        }
    }

    if (thingz_accel_shake.shaken)
        return TGZ_ACCEL_GESTURE_SHAKE;

    if (force < TGZ_ACCELEROMETER_FREEFALL_THRESHOLD)
        return TGZ_ACCEL_GESTURE_FREEFALL;

    if (force > TGZ_ACCELEROMETER_3G_THRESHOLD)
        return TGZ_ACCEL_GESTURE_3G;

    if (force > TGZ_ACCELEROMETER_6G_THRESHOLD)
        return TGZ_ACCEL_GESTURE_6G;

    if (force > TGZ_ACCELEROMETER_8G_THRESHOLD)
        return TGZ_ACCEL_GESTURE_8G;

    // Determine our posture.
    if (values[0] < (-1000 + TGZ_ACCELEROMETER_TILT_TOLERANCE))
        return TGZ_ACCEL_GESTURE_LEFT;

    if (values[0] > (1000 - TGZ_ACCELEROMETER_TILT_TOLERANCE))
        return TGZ_ACCEL_GESTURE_RIGHT;

    if (values[1] < (-1000 + TGZ_ACCELEROMETER_TILT_TOLERANCE))
        return TGZ_ACCEL_GESTURE_DOWN;

    if (values[1] > (1000 - TGZ_ACCELEROMETER_TILT_TOLERANCE))
        return TGZ_ACCEL_GESTURE_UP;

    if (values[2] > (1000 - TGZ_ACCELEROMETER_TILT_TOLERANCE))
        return TGZ_ACCEL_GESTURE_FACE_UP;

    if (values[2] < (-1000 + TGZ_ACCELEROMETER_TILT_TOLERANCE))
        return TGZ_ACCEL_GESTURE_FACE_DOWM;

    return TGZ_ACCEL_GESTURE_NONE;
}

/**
  * Updates the basic gesture recognizer. This performs instantaneous pose recognition, and also some low pass filtering to promote
  * stability.
  */
static void _thingz_accel_update_gesture(float* values, void* args)
{
    // Determine what it looks like we're doing based on the latest sample...
    thingz_accel_gesture g = _thingz_accel_instant_posture(values);
    DEBUG_printf("Gesture: %d\n", g);
    DEBUG_printf("current: %d\n", thingz_accel_current_gesture);
    DEBUG_printf("last: %d\n", thingz_accel_last_gesture);
    DEBUG_printf("sigma: %d\n", thingz_accel_sigma);
    // Perform some low pass filtering to reduce jitter from any detected effects
    if (g == thingz_accel_current_gesture)
    {
        if (thingz_accel_sigma < TGZ_ACCELEROMETER_GESTURE_DAMPING)
            thingz_accel_sigma++;
    }
    else
    {
        thingz_accel_current_gesture = g;
        thingz_accel_sigma = 0;
    }
    // ESP_LOGE(TAG, "koko");
    // If we've reached threshold, update our record and raise the relevant event...
    if (thingz_accel_current_gesture != thingz_accel_last_gesture && thingz_accel_sigma >= TGZ_ACCELEROMETER_GESTURE_DAMPING)
    {
        thingz_accel_obj_t *accel = (thingz_accel_obj_t*)args;
        // ESP_LOGE(TAG, "koko2 %p", accel);
        if(g != TGZ_ACCEL_GESTURE_NONE && accel && accel->gesture_callback[g]){
            // ESP_LOGE(TAG, "koko3 %d", thingz_accel_current_gesture);
            mp_obj_t f_args[1];
            f_args[0] = mp_obj_new_str(thingz_accel_gesture_to_str[thingz_accel_current_gesture], strlen(thingz_accel_gesture_to_str[thingz_accel_current_gesture]));
            // mp_type_fun_bc.ext->call(accel->gesture_callback[g], 1, 0, f_args);
            // ESP_LOGE(TAG, "koko4");
            mp_sched_schedule(accel->gesture_callback[g], f_args[0]);
        }
        thingz_accel_last_gesture = thingz_accel_current_gesture;
        thingz_accel_was_gesture |= 1 << thingz_accel_last_gesture;
        _thingz_accel_gesture_history_push(g);
    }
    // ESP_LOGE(TAG, "koko4");
}


void thingz_accel_compass_temp_init(thingz_accel_obj_t* accel, thingz_compass_obj_t* compass, thingz_temp_obj_t* temp, int8_t pinINT, int8_t pinINT2, int8_t pinDRDY){
   
    thingz_accel_init_gesture();

    int i;
    for(i = 0; i < TGZ_ACCEL_GESTURE_NONE; i++){
        accel->gesture_callback[i] = NULL;
    }
    
    common_thingz_accel_init(accel, pinINT, pinINT2, 0);
    common_thingz_compass_init(compass, pinDRDY, 0);
    common_thingz_temp_init(temp, 0);
}

void thingz_accel_compass_temp_deinit(thingz_accel_obj_t* accel, thingz_compass_obj_t* compass, thingz_temp_obj_t* temp){
    
    common_thingz_accel_deinit(accel);
    
    return;
}

void thingz_accel_new_data_callback(void *args){
    float new_values[3];
    common_thingz_accel_update_values(new_values);
    _thingz_accel_update_gesture(new_values, args);
    return;
}


//---------------------------- ACCEL ----------------------------
//NEW
static mp_obj_t mp_thingz_accel_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_accel_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_accel_obj_t));
    self->base.type = &thingz_accel_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_accel_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_accel_del_obj, mp_thingz_accel_del);

//GET X
static mp_obj_t mp_thingz_accel_get_x(mp_obj_t self_in) {
	thingz_accel_obj_t *self = MP_OBJ_TO_PTR(self_in);
    float values[3];
    common_thingz_accel_get_accel(self, values);
	return mp_obj_new_float(values[0]);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_accel_get_x_obj, mp_thingz_accel_get_x);

//GET Y
static mp_obj_t mp_thingz_accel_get_y(mp_obj_t self_in) {
	thingz_accel_obj_t *self = MP_OBJ_TO_PTR(self_in);
    float values[3];
    common_thingz_accel_get_accel(self, values);
	return mp_obj_new_float(values[1]);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_accel_get_y_obj, mp_thingz_accel_get_y);

//GET Z
static mp_obj_t mp_thingz_accel_get_z(mp_obj_t self_in) {
	thingz_accel_obj_t *self = MP_OBJ_TO_PTR(self_in);
    float values[3];
    common_thingz_accel_get_accel(self, values);
	return mp_obj_new_float(values[2]);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_accel_get_z_obj, mp_thingz_accel_get_z);

//GET
static mp_obj_t mp_thingz_accel_get(mp_obj_t self_in) {
	thingz_accel_obj_t *self = MP_OBJ_TO_PTR(self_in);
    float values[3];

    common_thingz_accel_get_accel(self, values);

    mp_obj_t mp_values[3];

    mp_values[0] = mp_obj_new_float(values[0]);
    mp_values[1] = mp_obj_new_float(values[1]);
    mp_values[2] = mp_obj_new_float(values[2]);

    return mp_obj_new_list(3, mp_values);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_accel_get_obj, mp_thingz_accel_get);

//CURRENT GESTURE
static mp_obj_t mp_thingz_accel_current_gesture(mp_obj_t self_in) {
	
    return mp_obj_new_str(thingz_accel_gesture_to_str[thingz_accel_last_gesture], strlen(thingz_accel_gesture_to_str[thingz_accel_last_gesture]));
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_accel_current_gesture_obj, mp_thingz_accel_current_gesture);

//IS GESTURE
static mp_obj_t mp_thingz_accel_is_gesture(mp_obj_t self_in, mp_obj_t gesture) {
	if(mp_obj_is_str(gesture)){
        const char* str = mp_obj_str_get_str(gesture);
        if(strcmp(str, thingz_accel_gesture_to_str[thingz_accel_last_gesture]) == 0){
            return mp_obj_new_bool(true);
        }
        return mp_obj_new_bool(false);
    }else{
        mp_raise_TypeError("argument 'gesture': must be a string");
    }               
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_accel_is_gesture_obj, mp_thingz_accel_is_gesture);

//WAS GESTURE
static mp_obj_t mp_thingz_accel_was_gesture(mp_obj_t self_in, mp_obj_t gesture) {
	int i;
    int8_t gesture_id = -1;
    if(mp_obj_is_str(gesture)){

        for(i = 0; i < TGZ_ACCEL_GESTURE_NUMBER_OF_GESTURE; i++){
            const char* str = mp_obj_str_get_str(gesture);
            if(strcmp(str, thingz_accel_gesture_to_str[i]) == 0){
                gesture_id = i;
                break;
            }
        }
        
        if(gesture_id != -1){
            if(thingz_accel_was_gesture & (1 << gesture_id)){
                thingz_accel_was_gesture &= ~(1 << gesture_id);
                return mp_obj_new_bool(true);
            }
            return mp_obj_new_bool(false);
        }else{
            mp_raise_ValueError("argument 'gesture': unknown gesture");
        }
        
    }else{
        mp_raise_TypeError("argument 'gesture': must be a string");
    }
    return mp_obj_new_bool(false);
}

MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_accel_was_gesture_obj, mp_thingz_accel_was_gesture);

//GET GESTURES
static mp_obj_t mp_thingz_accel_get_gestures(mp_obj_t self_in) {

    mp_obj_t gestures[TGZ_ACCEL_GESTURE_HISTORY_LENGTH];

    int i = 0;
    
    while(i < TGZ_ACCEL_GESTURE_HISTORY_LENGTH && thingz_accel_gesture_history[i] != TGZ_ACCEL_GESTURE_NONE){
        gestures[i] = mp_obj_new_str(thingz_accel_gesture_to_str[thingz_accel_gesture_history[i]], strlen(thingz_accel_gesture_to_str[thingz_accel_gesture_history[i]]));
        i++;
    }

    

    thingz_accel_current_history_pos = 0;
    int j = 0;
    for(j = 0; j < TGZ_ACCEL_GESTURE_HISTORY_LENGTH; j++){
        thingz_accel_gesture_history[j] = TGZ_ACCEL_GESTURE_NONE;
    }

    return mp_obj_new_list(i, gestures);
	
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_accel_get_gestures_obj, mp_thingz_accel_get_gestures);

//ON GESTURE
static mp_obj_t mp_thingz_accel_on_gesture(mp_obj_t self_in, mp_obj_t gesture, mp_obj_t function) {

    thingz_accel_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int i;
    int8_t gesture_id = -1;

    if(!mp_obj_is_type(function, &mp_type_fun_bc)) {
        mp_raise_TypeError("argument 'function': wrong prototype");
    }

    if(mp_obj_is_str(gesture)){

        for(i = 0; i < TGZ_ACCEL_GESTURE_NUMBER_OF_GESTURE; i++){
            const char* str = mp_obj_str_get_str(gesture);
            if(strcmp(str, thingz_accel_gesture_to_str[i]) == 0){
                gesture_id = i;
                break;
            }
        }
        
        if(gesture_id != -1){
            mp_obj_fun_bc_t *fun = MP_OBJ_TO_PTR(function);
            self->gesture_callback[gesture_id] = fun;
        }else{
            mp_raise_ValueError("argument 'gesture': unknown gesture");
        }
        
    }else{
        mp_raise_TypeError("argument 'gesture': must be a string");
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(mp_thingz_accel_on_gesture_obj, mp_thingz_accel_on_gesture);

static const mp_map_elem_t thingz_accel_local_dict_table[] = {
	
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__), (mp_obj_t)&mp_thingz_accel_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_x),   (mp_obj_t)&mp_thingz_accel_get_x_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_y),   (mp_obj_t)&mp_thingz_accel_get_y_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_z),   (mp_obj_t)&mp_thingz_accel_get_z_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_values),     (mp_obj_t)&mp_thingz_accel_get_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_current_gesture),     (mp_obj_t)&mp_thingz_accel_current_gesture_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_is_gesture),     (mp_obj_t)&mp_thingz_accel_is_gesture_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_was_gesture),     (mp_obj_t)&mp_thingz_accel_was_gesture_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_gestures),     (mp_obj_t)&mp_thingz_accel_get_gestures_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_on_gesture), (mp_obj_t)&mp_thingz_accel_on_gesture_obj },    
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_accel_local_dict,
	thingz_accel_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_accel_type,
    MP_QSTR_Accel,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_accel_make_new,
    locals_dict, &mp_thingz_accel_local_dict
);

//------------------------------- COMPASS ----------------------------------------

//NEW
static mp_obj_t mp_thingz_compass_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_compass_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_compass_obj_t));
    self->base.type = &thingz_compass_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_compass_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_compass_del_obj, mp_thingz_compass_del);

//GET X
static mp_obj_t mp_thingz_compass_get_x(mp_obj_t self_in) {
	thingz_compass_obj_t *self = MP_OBJ_TO_PTR(self_in);
    thingz_compass_data_t data;
    common_thingz_compass_get_gauss(self, 0, &data);
	return mp_obj_new_float(data.x/10.0);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_compass_get_x_obj, mp_thingz_compass_get_x);

//GET Y
static mp_obj_t mp_thingz_compass_get_y(mp_obj_t self_in) {
	thingz_compass_obj_t *self = MP_OBJ_TO_PTR(self_in);
    thingz_compass_data_t data;
    common_thingz_compass_get_gauss(self, 0, &data);
    //convert to uT
	return mp_obj_new_float(data.y/10.0);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_compass_get_y_obj, mp_thingz_compass_get_y);

//GET Z
static mp_obj_t mp_thingz_compass_get_z(mp_obj_t self_in) {
	thingz_compass_obj_t *self = MP_OBJ_TO_PTR(self_in);
    thingz_compass_data_t data;
    common_thingz_compass_get_gauss(self, 0, &data);
	return mp_obj_new_float(data.z/10.0);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_compass_get_z_obj, mp_thingz_compass_get_z);

//GET
static mp_obj_t mp_thingz_compass_get(mp_obj_t self_in) {
	thingz_compass_obj_t *self = MP_OBJ_TO_PTR(self_in);
    thingz_compass_data_t data;
    common_thingz_compass_get_gauss(self, 0, &data);

    mp_obj_t mp_values[3];
    mp_values[0] = mp_obj_new_float(data.x/10.0);
    mp_values[1] = mp_obj_new_float(data.y/10.0);
    mp_values[2] = mp_obj_new_float(data.z/10.0);

    return mp_obj_new_list(3, mp_values);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_compass_get_obj, mp_thingz_compass_get);

//HEADING
static mp_obj_t mp_thingz_compass_heading(mp_obj_t self_in) {
	thingz_compass_obj_t *self = MP_OBJ_TO_PTR(self_in);
    thingz_compass_data_t data;
    common_thingz_compass_get_gauss(self, 0, &data);
    float heading = (double)atan2f(data.x, data.y)*(double)(180.0/M_PI);
    if(heading > 360)
		heading -= 360;
	else if(heading < 0)
		heading += 360;
	return mp_obj_new_float(heading);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_compass_heading_obj, mp_thingz_compass_heading);

//CALIBRATE
static mp_obj_t mp_thingz_compass_calibrate(mp_obj_t self_in, mp_obj_t duration, mp_obj_t sample){
    thingz_compass_obj_t *self = MP_OBJ_TO_PTR(self_in);
    common_thingz_compass_calibrate(self, mp_obj_get_int(duration), mp_obj_get_int(sample));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(mp_thingz_compass_calibrate_obj, mp_thingz_compass_calibrate);

static const mp_map_elem_t thingz_compass_local_dict_table[] = {
	
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__),         (mp_obj_t)&mp_thingz_compass_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_x),           (mp_obj_t)&mp_thingz_compass_get_x_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_y),           (mp_obj_t)&mp_thingz_compass_get_y_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_z),           (mp_obj_t)&mp_thingz_compass_get_z_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_values),      (mp_obj_t)&mp_thingz_compass_get_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_heading),         (mp_obj_t)&mp_thingz_compass_heading_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_calibrate),       (mp_obj_t)&mp_thingz_compass_calibrate_obj },

};

static MP_DEFINE_CONST_DICT (
	mp_thingz_compass_local_dict,
	thingz_compass_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_compass_type,
    MP_QSTR_Compass,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_compass_make_new,
    locals_dict, &mp_thingz_compass_local_dict
);

//------------------------------- TEMP ----------------------------------------

//NEW
static mp_obj_t mp_thingz_temp_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_temp_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_temp_obj_t));
    self->base.type = &thingz_temp_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_temp_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_temp_del_obj, mp_thingz_temp_del);

//GET TEMP
static mp_obj_t mp_thingz_temp_get_temp(mp_obj_t self_in) {
	thingz_temp_obj_t *self = MP_OBJ_TO_PTR(self_in);

    float temp = common_thingz_accel_compass_temp_get_temp(self);

	return mp_obj_new_float(temp);
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_temp_get_temp_obj, mp_thingz_temp_get_temp);

static const mp_map_elem_t thingz_temp_local_dict_table[] = {
	
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__), (mp_obj_t)&mp_thingz_temp_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get_temperature),   (mp_obj_t)&mp_thingz_temp_get_temp_obj },
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_temp_local_dict,
	thingz_temp_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_temp_type,
    MP_QSTR_Temperature,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_temp_make_new,
    locals_dict, &mp_thingz_temp_local_dict
);