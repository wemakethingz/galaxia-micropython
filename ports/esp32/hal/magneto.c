#include "mpconfigboard.h"

#include "common-thingz/thingz_accel/thingz_accel.h"


#include "driver/gpio.h"
#include "driver/i2c.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "string.h"
#include "math.h"

#include "lib/qmc6310u/qmc6310u.h"
#include "lib/qmc6309/qmc6309.h"

#include "common-thingz/thingz_memory/thingz_memory.h"
#include "common-thingz/thingz_i2c/thingz_i2c.h"

#define QMC6310U_ADDR 0x1C << 1
#define QMC6309_ADDR 0x7C << 1

#define COMPASS_FREQ 10.0f
#define COMPASS_TIME_BETWEEN_REQUEST 1.0f/COMPASS_FREQ

// static const char* TAG = "THGZ_GALAXIA_COMPASS";

typedef struct thingz_compass_calibration_v1{
    float hardiron_offset[3];
    float softiron_scale[3];
} thingz_compass_calibration_v1_t;

typedef struct thingz_compass_calibration_v2{
    //saved as float in eeprom
    double softiron_matrix[3][3];
    float hardiron_offset[3];
} thingz_compass_calibration_v2_t;

typedef struct thingz_compass_calibration_method{
    uint8_t used_version;
    union{
        thingz_compass_calibration_v1_t v1;
        thingz_compass_calibration_v2_t v2;
    };
}thingz_compass_calibration_method_t;

static uint8_t bus_id;

static thingz_compass_data_t last_values;
static int64_t last_value_timestamp;
static qmc630u_ctx_t ctx_compass_qmc630u;
static QMC6309_ctx_t ctx_compass_qmc6309;

static uint32_t errors_count; 

static uint8_t compass_addr;
static uint8_t compass_range;

static thingz_compass_calibration_method_t calibration;

static const thingz_compass_calibration_v2_t default_calibration_qmc6310u = {
    {{1.601022, 0, 0}, {-0.336627, 1.392834, 0}, {0, 0, 6.549427}}, 
    {-161.899996, -329.866672, -84.533339}
};

static const thingz_compass_calibration_v2_t default_calibration_qmc6309 = {
    {{1.461751, 0, 0}, {-0.135340, 1.595138, 0}, {0, 0, 7.643435}}, 
    {-126.249993, -388.249989, -370.875001}
};

static int32_t i2c_write(uint8_t reg, uint8_t *bufp, uint16_t len){
    esp_err_t error = common_thingz_i2c_write(compass_addr, bus_id, reg, bufp, len);
    if(error == ESP_OK){
        errors_count = 0;
    }else{
        errors_count++;
    }
    return error;
}

static int32_t i2c_read(uint8_t reg, uint8_t *bufp, uint16_t len){
    esp_err_t error = common_thingz_i2c_read(compass_addr, bus_id, reg, bufp, len);
    if(error == ESP_OK){
        errors_count = 0;
    }else{
        errors_count++;
    }
    return error;
}

static void matrix_cholesky(double m[3][3], double cholesky[3][3], uint8_t dimension){
    for(int i = 0; i < dimension; i++){
        for(int j = 0; j < i+1; j++){
            double sum = 0;
            for (int k = 0; k < j; k++){
                sum += cholesky[i][k] * cholesky[j][k];
            }
            if(j == i){
                cholesky[i][j] = sqrtf(m[i][i] - sum);
            }else{
                cholesky[i][j] = (m[i][j] - sum) / cholesky[j][j];
            }
        }
    }
}

// static double matrix_det(double matrix[3][3]){
//     return matrix[0][0]*(matrix[1][1]*matrix[2][2]-matrix[1][2]*matrix[2][1]) - matrix[0][1]*(matrix[1][0]*matrix[2][2]-matrix[1][2]*matrix[2][0]) + matrix[0][2]*(matrix[1][0]*matrix[2][1]-matrix[1][1]*matrix[2][0]);
// }

static double matrix_det_2d(double matrix[3][3]){
    return matrix[0][0]*matrix[1][1] - matrix[0][1]*matrix[1][0];
}

static void matrix_product(double matrix[3], double matrix2[3][3], double result[3]){
    double p[3]={0,0,0};
    for(int i = 0; i<3; i++){
        for(int j = 0; j < 3; j++){
            p[i] += matrix[j]*matrix2[j][i];
        }
    }
    for(int i = 0; i < 3; i++){
        result[i]=p[i];
    }
}

// static void matrix_inverse(double matrix[3][3], double inverse[3][3]){
    
//     // C= ​ei−fh −(di−fg) dh−eg
//     //  −(bi−ch) ai−cg −(ah−bg)
//     //   ​(bf−ce)−(af−cd)ae−bd​
//     //matrice adjointe des cofacteurs
//     inverse[0][0] = matrix[1][1]*matrix[2][2]-matrix[1][2]*matrix[2][1];
//     inverse[0][1] = (double)-1.0*(matrix[1][0]*matrix[2][2]-matrix[1][2]*matrix[2][0]);
//     inverse[0][2] = matrix[1][0]*matrix[2][1]-matrix[1][1]*matrix[2][0];
//     inverse[1][0] = (double)-1.0*(matrix[0][1]*matrix[2][2]-matrix[0][2]*matrix[2][1]);
//     inverse[1][1] = matrix[0][0]*matrix[2][2]-matrix[0][2]*matrix[2][0];
//     inverse[1][2] = (double)-1.0*(matrix[0][0]*matrix[2][1]-matrix[0][1]*matrix[2][0]);
//     inverse[2][0] = matrix[0][1]*matrix[1][2]-matrix[0][2]*matrix[1][1];
//     inverse[2][1] = (double)-1.0*(matrix[0][0]*matrix[1][2]-matrix[0][2]*matrix[1][0]);
//     inverse[2][2] = matrix[0][0]*matrix[1][1]-matrix[0][1]*matrix[1][0];

//     double det = matrix_det(matrix);

//     for(int i = 0; i < 3; i++){
//         for(int j = 0; j < 3; j++){
//             inverse[i][j] = inverse[i][j] / det;
//         }
//     }
// }

static void matrix_inverse_2d(double matrix[3][3], double inverse[3][3]){
    
    inverse[0][0] = matrix[1][1];
    inverse[0][1] = (double)-1.0*matrix[0][1];
    inverse[1][0] = (double)-1.0*matrix[1][0];
    inverse[1][1] = matrix[0][0];

    double det = matrix_det_2d(matrix);

    for(int i = 0; i < 2; i++){
        for(int j = 0; j < 2; j++){
            inverse[i][j] = inverse[i][j] / det;
            // mp_printf(MP_PYTHON_PRINTER, "m %.6f d %.6f i %.6f\n", matrix[i][j], det, inverse[i][j]);
        }
    }
}

static void matrix_covariance(thingz_compass_data_t *measurements, int samples, double covariance[3][3], uint8_t dimension){
    for(int j = 0; j < 3; j++){
        for(int k = 0; k < 3; k++){
            covariance[j][k] = 0;
        }
    }
    for (int i = 0; i < samples; i++) {    
        double m[3] = {measurements[i].x, measurements[i].y, measurements[i].z};     
        for(int j = 0; j < dimension; j++){
            for(int k = 0; k < dimension; k++){
                // mp_printf(MP_PYTHON_PRINTER, "I %d J %d K %d covariance[j][k] %.6f m[j] %.6f m[k] %.6f m[j]*m[k] %.6f\n", i, j, k, covariance[j][k], m[j], m[k], m[j]*m[k]);
                covariance[j][k] += (m[j]*m[k]);
            }
        }
    }
    for(int j = 0; j < 3; j++){
        for(int k = 0; k < 3; k++){
            covariance[j][k] /= (double)samples;
        }
    }
}

static void _calibrate_v1(thingz_compass_data_t* values, thingz_compass_data_t* raw){

    values->x = raw->x-(double)calibration.v1.hardiron_offset[0];
    values->y = raw->y-(double)calibration.v1.hardiron_offset[1];
    values->z = raw->z-(double)calibration.v1.hardiron_offset[2];

    values->x = values->x*(double)calibration.v1.softiron_scale[0];//-5;
    values->y = values->y*(double)calibration.v1.softiron_scale[1];//+190;
    values->z = values->z*(double)calibration.v1.softiron_scale[2];
}

static void _calibrate_v2(thingz_compass_data_t* values, thingz_compass_data_t* raw){
    // mp_printf(MP_PYTHON_PRINTER, "raw %.6f %.6f %.6f\n",  raw->x, raw->y, raw->z);
    double m[3] = {raw->x-(double)calibration.v2.hardiron_offset[0], raw->y-(double)calibration.v2.hardiron_offset[1], raw->z-(double)calibration.v2.hardiron_offset[2]};
    // mp_printf(MP_PYTHON_PRINTER, "offseted %.6f %.6f %.6f\n",  m[0], m[1], m[2]);
    matrix_product(m, calibration.v2.softiron_matrix, m);
    // mp_printf(MP_PYTHON_PRINTER, "softed %.6f %.6f %.6f\n",  m[0], m[1], m[2]);
    values->x = m[0];
    values->y = m[1];
    values->z = m[2];
    // mp_printf(MP_PYTHON_PRINTER, "values %.6f %.6f %.6f\n",  values->x, values->y, values->z);
}

static void _calibrate(thingz_compass_data_t* values, thingz_compass_data_t* raw){
    switch(calibration.used_version){
        case 1:
            _calibrate_v1(values, raw);
        break;
        case 2:
            _calibrate_v2(values, raw);
        break;
        default:
            values->x = raw->x;
            values->y = raw->y;
            values->z = raw->z;
        break;
    }
}

static void _used_old_value(thingz_compass_data_t* values){
    _calibrate(values, &last_values);
}

static void _update_old_values(thingz_compass_data_t* values){
    last_values.x = values->x;
    last_values.y = values->y;
    last_values.z = values->z;
}

static double _check_calibration_value(float value, double replacement){
    if(*((uint32_t*)(float*)&value) == 0xFFFFFFFF || isnan(value)){
        return replacement;
    }
    return (double)value;
}

static void _load_calibration_data(int8_t version){
    const thingz_compass_calibration_v2_t* default_calibration;

    if(version == THINGZ_VERSION_1_0_7){
        default_calibration = &default_calibration_qmc6309;
    }else{
        default_calibration = &default_calibration_qmc6310u;
    }

    if(thingz_memory_get_setting("compass_hard_x", calibration.v2.hardiron_offset) == ESP_OK){
        calibration.v2.hardiron_offset[0] = _check_calibration_value(calibration.v2.hardiron_offset[0], default_calibration->hardiron_offset[0]);
        if(fabs(calibration.v2.hardiron_offset[0])  <= (double)0.0){
            calibration.v2.hardiron_offset[0] = default_calibration->hardiron_offset[0];
        }
    }else{
        calibration.v2.hardiron_offset[0] = default_calibration->hardiron_offset[0];
    }

    if(thingz_memory_get_setting("compass_hard_y", calibration.v2.hardiron_offset+1) == ESP_OK){
        calibration.v2.hardiron_offset[1] = _check_calibration_value(calibration.v2.hardiron_offset[1], default_calibration->hardiron_offset[1]);
        if(fabs(calibration.v2.hardiron_offset[1])  <= (double)0.1){
            calibration.v2.hardiron_offset[1] = default_calibration->hardiron_offset[1];
        }
    }else{
        calibration.v2.hardiron_offset[1] = default_calibration->hardiron_offset[1];
    }

    if(thingz_memory_get_setting("compass_hard_z", calibration.v2.hardiron_offset+2) == ESP_OK){
        calibration.v2.hardiron_offset[2] = _check_calibration_value(calibration.v2.hardiron_offset[2], default_calibration->hardiron_offset[2]);
        if(fabs(calibration.v2.hardiron_offset[2])  <= (double)0.1){
            calibration.v2.hardiron_offset[2] = default_calibration->hardiron_offset[2];
        }
    }else{
        calibration.v2.hardiron_offset[2] = default_calibration->hardiron_offset[2];
    }
    float matrix[3][3];
    if(thingz_memory_get_setting("compass_soft_m", matrix) == ESP_OK){
        for(int i = 0; i < 3; i++){
            for(int j = 0; j < 3; j++){
                calibration.v2.softiron_matrix[i][j] = _check_calibration_value(matrix[i][j], default_calibration->softiron_matrix[i][j]);
            }
        }
    }else{
        for(int i = 0; i < 3; i++){
            for(int j = 0; j < 3; j++){
                calibration.v2.softiron_matrix[i][j] = default_calibration->softiron_matrix[i][j];
            }
        }
    }

    calibration.used_version = 2;
    // mp_printf(MP_PYTHON_PRINTER, "hard %.6f %.6f %.6f\n", (double)calibration.v2.hardiron_offset[0], (double)calibration.v2.hardiron_offset[1], (double)calibration.v2.hardiron_offset[2]);
    // mp_printf(MP_PYTHON_PRINTER, "soft %.6f %.6f %.6f\n%.6f %.6f %.6f\n%.6f %.6f %.6f\n", (double)calibration.v2.softiron_matrix[0][0], (double)calibration.v2.softiron_matrix[0][1], (double)calibration.v2.softiron_matrix[0][2],(double)calibration.v2.softiron_matrix[1][0], (double)calibration.v2.softiron_matrix[1][1], (double)calibration.v2.softiron_matrix[1][2],(double)calibration.v2.softiron_matrix[2][0], (double)calibration.v2.softiron_matrix[2][1], (double)calibration.v2.softiron_matrix[2][2]);
    
}

void common_thingz_compass_init(thingz_compass_obj_t* compass, int8_t pinDRDY, uint8_t i2c_bus_id){   

    compass->pinDRDY = pinDRDY;
    compass->base.type = &thingz_compass_type;

    bus_id = i2c_bus_id;

    uint32_t setting = 0;
    int8_t version;

    if(thingz_memory_get_setting("version", &setting) == -1){
        version = -1;
    }else{
        version = setting;
    }

    _load_calibration_data(version);

    if(version == THINGZ_VERSION_1_0_7){
        // pcb 1.0.7
        compass_addr = QMC6309_ADDR;
    }else{
        compass_addr = QMC6310U_ADDR;
    }

    // mp_printf(MP_PYTHON_PRINTER, "hx %f hy %f hz %f sx %f sy %f sz %f\n", (double)hardiron_offset[0], (double)hardiron_offset[1], (double)hardiron_offset[2], (double)softiron_scale[0], (double)softiron_scale[1], (double)softiron_scale[2]);

    last_values.x = 0;
    last_values.y = 0;
    last_values.z = 0;

    last_value_timestamp = 0;

    ctx_compass_qmc630u.read_reg = i2c_read;
    ctx_compass_qmc630u.write_reg = i2c_write;

    ctx_compass_qmc6309.read_reg = i2c_read;
    ctx_compass_qmc6309.write_reg = i2c_write;


    errors_count = 0;

    if(compass_addr == QMC6310U_ADDR){
        compass_range = QMC630U_RANGE_2;
        qmc630u_init(&ctx_compass_qmc630u, compass_range, COMPASS_FREQ);
    }else{
        compass_range = QMC6309_RANGE_8;
        QMC6309_init(&ctx_compass_qmc6309, compass_range, COMPASS_FREQ);
    }
    
}

void common_thingz_compass_get_gauss(thingz_compass_obj_t* compass, uint8_t raw, thingz_compass_data_t* values){
    // mp_printf(MP_PYTHON_PRINTER, "hard %.6f %.6f %.6f\n", calibration.v2.hardiron_offset[0], calibration.v2.hardiron_offset[1], calibration.v2.hardiron_offset[2]);
    // mp_printf(MP_PYTHON_PRINTER, "soft %.6f %.6f %.6f\n%.6f %.6f %.6f\n%.6f %.6f %.6f\n", calibration.v2.softiron_matrix[0][0], calibration.v2.softiron_matrix[0][1], calibration.v2.softiron_matrix[0][2],calibration.v2.softiron_matrix[1][0], calibration.v2.softiron_matrix[1][1], calibration.v2.softiron_matrix[1][2],calibration.v2.softiron_matrix[2][0], calibration.v2.softiron_matrix[2][1], calibration.v2.softiron_matrix[2][2]);
    // mp_printf(MP_PYTHON_PRINTER, "scale %.6f\n", calibration.v2.scale);
    thingz_compass_data_t data;
    data.x = 0;
    data.y = 0;
    data.z = 0;
    values->x = 0;
    values->y = 0;
    values->z = 0;
    esp_err_t error = 0;
    // raw = 1;
    if(!raw && esp_timer_get_time() - last_value_timestamp < (COMPASS_TIME_BETWEEN_REQUEST)*1000000){
        _used_old_value(values);
        return;
    }

    float d[3];
    if(compass_addr == QMC6310U_ADDR){
        error = qmc630u_get_mgauss(&ctx_compass_qmc630u, d, compass_range);
    }else{
        error = QMC6309_get_mgauss(&ctx_compass_qmc6309, d, compass_range);
        //fix axis orientation
        // raw[0] = raw[0]*-1;
        // raw[1] = raw[1]*-1;
        d[2] = d[2]*-1;
    }
    data.x = d[0];
    data.y = d[1];
    data.z = d[2];

    if(error == ESP_OK){
        last_value_timestamp = esp_timer_get_time();
        _update_old_values(&data);
        _calibrate(values, &data);
    }else{
        _used_old_value(values);
        return;
    }
}

// Fonction principale de calibration
static void calibrate_magnetometer(thingz_compass_data_t *measurements, int samples, double max[3], double min[3]) {
    double covariance[3][3], hardiron[3], cholesky[3][3], softiron[3][3], scale[2];
    
    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 3; j++){
            covariance[i][j] = 0;
            cholesky[i][j] = 0;
            softiron[i][j] = 0;
        }
    }
    scale[0] = max[0];
    //Find offsets
    for(int i = 0; i < 3; i++){
        if(fabs(max[i]) > scale[0]){
            scale[0] = fabs(max[i]);
        }
        if(fabs(min[i]) > scale[0]){
            scale[0] = fabs(min[0]);
        }
        hardiron[i] = (max[i]+min[i])/(double)2.0;
    }
    //Apply offset
    for (int i = 0; i < samples; i++) {
        measurements[i].x-=hardiron[0];
        measurements[i].y-=hardiron[1]; 
        measurements[i].z-=hardiron[2];
    }

    max[2] = measurements[0].z;
    min[2] = measurements[0].z;
    for (int i = 0; i < samples; i++) {
        if(measurements[i].z < min[2]){
            min[2] = measurements[i].z;
        }else if(measurements[i].z > max[2]){
            max[2] = measurements[i].z;
        }
    }
    matrix_covariance(measurements, samples, covariance, 2);
    covariance[0][2] = 0;
    covariance[1][2] = 0;
    covariance[2][0] = 0;
    covariance[2][1] = 0;
    covariance[2][2] = 1;

    // double determinantDataRaw = matrix_det(covariance);
    // double traceDataRaw = covariance[0][0]+covariance[1][1]+covariance[2][2];
    // mp_printf(MP_PYTHON_PRINTER, "covariance:\n");
    // for(int i = 0; i < 3; i++){
    //     for(int j = 0; j < 3; j++){
    //         mp_printf(MP_PYTHON_PRINTER, "%.6f\t", covariance[i][j]);
    //     }
    //     mp_printf(MP_PYTHON_PRINTER, "\n");
    // }
    matrix_cholesky(covariance, cholesky, 2);
    // mp_printf(MP_PYTHON_PRINTER, "cholesky:\n");
    // for(int i = 0; i < 3; i++){
    //     for(int j = 0; j < 3; j++){
    //         mp_printf(MP_PYTHON_PRINTER, "%.6f\t", cholesky[i][j]);
    //     }
    //     mp_printf(MP_PYTHON_PRINTER, "\n");
    // }

    matrix_inverse_2d(cholesky, softiron);
    softiron[2][2] = (double)2.0/(max[2]-min[2]);
    
    scale[1] = measurements[0].x;
    for(int i = 0; i < samples; i++){
        double d[3] = {measurements[i].x, measurements[i].y, measurements[i].z};
        matrix_product(d, softiron, d);
        measurements[i].x = d[0];
        measurements[i].y = d[1];
        measurements[i].z = d[2];
        
        if(fabs(measurements[i].x) > scale[1]){
            scale[1] = fabs(measurements[i].x);
        }
        if(fabs(measurements[i].y) > scale[1]){
            scale[1] = fabs(measurements[i].y);
        }
        if(fabs(measurements[i].z) > scale[1]){
            scale[1] = fabs(measurements[i].z);
        }

    }

    // double determinantDataCorrection = matrix_det(covariance);
    // double traceDataCorrection = covariance[0][0]+covariance[1][1]+covariance[2][2];
    // double s = sqrtf(traceDataRaw/traceDataCorrection);

    // for(int i = 0; i < 3; i++){
    //     for(int j = 0; j < 3; j++){
    //         scale[i][j] = softiron[i][j] * s;
    //     }
    // }

    calibration.used_version = 2;
    // mp_printf(MP_PYTHON_PRINTER, "%.6f %.6f\n", (double)scale[0], scale[1]);

    scale[0] = (scale[0] / scale[1]);
    float sm[3][3];
    for(int i = 0; i < 3; i++){
        calibration.v2.hardiron_offset[i] = hardiron[i];
        for(int j = 0; j < 3; j++){
            // calibration.v2.softiron_matrix[i][j] = softiron[i][j];
            calibration.v2.softiron_matrix[i][j] = softiron[i][j]*scale[0];
            sm[i][j] = (float)calibration.v2.softiron_matrix[i][j];
        }
    }

    // mp_printf(MP_PYTHON_PRINTER, "Offsets : bx = %.6f, by = %.6f, bz = %.6f\n", (double)calibration.v2.hardiron_offset[0], (double)calibration.v2.hardiron_offset[1], (double)calibration.v2.hardiron_offset[2]);
    // mp_printf(MP_PYTHON_PRINTER,"Det(c) = %.6f Det(c') = %.6f s1 %.6f s2 %.6f s3 %.6f\n", determinantDataRaw,determinantDataCorrection, covariance[0][0], covariance[1][1], covariance[2][2]);
    // mp_printf(MP_PYTHON_PRINTER, "correction:\n");
    
    // for(int i = 0; i < 3; i++){
    //     for(int j = 0; j < 3; j++){
    //         mp_printf(MP_PYTHON_PRINTER, "%.6f\t", (double)calibration.v2.softiron_matrix[i][j]);
    //     }
    //     mp_printf(MP_PYTHON_PRINTER, "\n");
    // }
    thingz_memory_set_setting("compass_hard_x", calibration.v2.hardiron_offset);
    thingz_memory_set_setting("compass_hard_y", calibration.v2.hardiron_offset+1);
    thingz_memory_set_setting("compass_soft_m", sm);
    thingz_memory_set_setting("compass_hard_z", calibration.v2.hardiron_offset+2);
}

// static void build_least_squares_matrix(thingz_compass_data_t *data, double A[100][10], double B[10]) {
   
// }

// // Fonction pour résoudre AX = B en utilisant l'élimination de Gauss
// static void gauss_elimination(double **A, double *B, double *X) {
//     int i, j, k;
//     double ratio;
//     printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@GAUSS\n");
//     // Étape 1: Conversion de A en une matrice triangulaire supérieure
//     for (i = 0; i < 10; i++) {
//         RUN_BACKGROUND_TASKS;
//         for (j = i + 1; j < 100; j++) {
//             RUN_BACKGROUND_TASKS;
//             ratio = A[j][i] / A[i][i];

//             for (k = 0; k < 10; k++) {
//                 A[j][k] -= ratio * A[i][k];
//             }
//             B[j] -= ratio * B[i];
//         }
//     }
//     printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@GAUSS\n");
//     // Étape 2: Résolution par remontée
//     for (i = 10 - 1; i >= 0; i--) {
//         RUN_BACKGROUND_TASKS;
//         X[i] = B[0];

//         for (j = i + 1; j < 10; j++) {
//             RUN_BACKGROUND_TASKS;
//             X[i] -= A[i][j] * X[j];
//         }
//         X[i] /= A[i][i];
//     }
// }


void common_thingz_compass_calibrate(thingz_compass_obj_t* compass, int duration, int sample){
    thingz_compass_data_t *data;
    // double A[50][10];
    // double B[50];
    int i;
    uint8_t old_calibration_version = calibration.used_version;
    uint32_t sleep = (double)((double)duration/(double)sample)*1000;
    // mp_printf(MP_PYTHON_PRINTER,"sleep %d\n", sleep);
    calibration.used_version = 0;
    data = NULL;
    data = m_malloc(sizeof(thingz_compass_data_t)*sample);
    if(!data){
        mp_printf(MP_PYTHON_PRINTER,"Allocation failed, try with lower sample\n");
        calibration.used_version = old_calibration_version;
        return;
    }
    double max[3]={0}, min[3]={0};
    for(i = 0; i < sample; i++){
        common_thingz_compass_get_gauss(compass, 1, data+i);
        if(i == 0){
            max[0] = data[i].x;
            max[1] = data[i].y;
            max[2] = data[i].z;
            min[0] = data[i].x;
            min[1] = data[i].y;
            min[2] = data[i].z;
        }else{
            if(data[i].x < min[0]){
                min[0] = data[i].x;
            }else if(data[i].x > max[0]){
                max[0] = data[i].x;
            }
            if(data[i].y < min[1]){
                min[1] = data[i].y;
            }else if(data[i].y > max[1]){
                max[1] = data[i].y;
            }
            if(data[i].z < min[2]){
                min[2] = data[i].z;
            }else if(data[i].z > max[2]){
                max[2] = data[i].z;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(sleep));
    }
    // for(i = 0; i < sample; i++){
    //     mp_printf(MP_PYTHON_PRINTER, "%.6f,%.6f,%.6f\n", (double)data[i].x, (double)data[i].y, (double)data[i].z);
    // }
    calibrate_magnetometer(data, sample, max, min);
    m_free(data);

}