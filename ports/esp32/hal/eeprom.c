#include "common-thingz/thingz_memory/thingz_memory.h"
#include "esp_err.h"
#include "nvs.h"
#include "string.h"
#include "py/mpprint.h"
#include "FreeRTOS.h"
#include "freertos/task.h"
#include <fcntl.h>
#include <stdint.h>

#define THINGZ_MEMORY_EEPROM_EXPECTED_VERSION 1


typedef struct thingz_memory_eeprom_info {
    uint8_t expected_protocol_version;
    uint8_t eeprom_corrupt;
    uint8_t eeprom_present;
    uint8_t eeprom_protocol_version;
} thingz_memory_eeprom_info_t;

static thingz_memory_eeprom_info_t thingz_eeprom_info;

enum thingz_eeprom_map_v0 {
    EEPROM_V0_VERSION = 0,
    EEPROM_V0_DISPLAY,
    EEPROM_V0_COMPASS_HARD_X,
    EEPROM_V0_COMPASS_HARD_Y = EEPROM_V0_COMPASS_HARD_X+4,
    EEPROM_V0_COMPASS_HARD_Z = EEPROM_V0_COMPASS_HARD_Y+4,
    EEPROM_V0_COMPASS_SOFT_X = EEPROM_V0_COMPASS_HARD_Z+4,
    EEPROM_V0_COMPASS_SOFT_Y = EEPROM_V0_COMPASS_SOFT_X+4,
    EEPROM_V0_COMPASS_SOFT_Z = EEPROM_V0_COMPASS_SOFT_Y+4,
    EEPROM_V0_LENGTH
};

enum thingz_eeprom_map_v1 {
    EEPROM_V1_EEPROM_VERSION = 0,
    EEPROM_V1_VERSION,
    EEPROM_V1_DISPLAY,
    EEPROM_V1_COMPASS_HARD_X,
    EEPROM_V1_COMPASS_HARD_Y = EEPROM_V1_COMPASS_HARD_X+4,
    EEPROM_V1_COMPASS_HARD_Z = EEPROM_V1_COMPASS_HARD_Y+4,
    EEPROM_V1_COMPASS_SOFT_X = EEPROM_V1_COMPASS_HARD_Z+4,
    EEPROM_V1_COMPASS_SOFT_MATRIX = EEPROM_V1_COMPASS_SOFT_X,
    EEPROM_V1_COMPASS_SOFT_Y = EEPROM_V1_COMPASS_SOFT_X+4,
    EEPROM_V1_COMPASS_SOFT_Z = EEPROM_V1_COMPASS_SOFT_Y+4,
    // EEPROM_V1_COMPASS_SOFT_MATRIX = EEPROM_V1_COMPASS_SOFT_Z+4,
    // EEPROM_V1_COMPASS_SOFT_MATRIX_SCALE = EEPROM_V1_COMPASS_SOFT_MATRIX+4*9,
    EEPROM_V1_LENGTH
};

typedef struct thingz_memory_settings{
    uint8_t hardware_version;
    double hardiron[3];
    double softiron[3][3];
} thingz_eeprom_settings_t;

static int16_t _thingz_memory_map_setting_to_eeprom_addr(uint8_t setting){
    if(thingz_eeprom_info.eeprom_protocol_version == THINGZ_MEMORY_EEPROM_EXPECTED_VERSION){
        return setting;
    }

    if(setting == EEPROM_V1_EEPROM_VERSION){
        return -1;
    }

    return setting - 1;
}

static int _thingz_memory_get_eeprom_setting_or_fallback(uint8_t setting, const char* name, void* value, size_t len, size_t fallback_len){
    uint8_t needFallback = 1;
    // mp_printf(MP_PYTHON_PRINTER, "GET %s setting: %d present: %d\n", name, setting, thingz_eeprom_info.eeprom_present);
    if(thingz_eeprom_info.eeprom_present){
        if(thingz_memory_read_array_eeprom(_thingz_memory_map_setting_to_eeprom_addr(setting), (uint8_t*)value, len) == ESP_OK){
            needFallback = 0;
            // for(size_t i = 0; i < len; i++){
            //     mp_printf(MP_PYTHON_PRINTER, "%d ", *(uint8_t*)(value+i));
            // }
            // mp_printf(MP_PYTHON_PRINTER, "\n");
        }
        // else{
        //     mp_printf(MP_PYTHON_PRINTER, "FAIL\n");
        // }
    }
    if(needFallback){
        nvs_handle* nvsHandle;
        nvsHandle = thingz_memory_get_handle();
        if(nvs_get_blob(*nvsHandle, name, value, &fallback_len) == ESP_OK){
            return ESP_OK;
        }
    }else{
        return ESP_OK;
    }
    return ESP_FAIL;
}

static int _thingz_memory_set_eeprom_setting_or_fallback(uint8_t setting, const char* name, void* value, size_t len, size_t fallback_len){
    uint8_t needFallback = 0;
    // mp_printf(MP_PYTHON_PRINTER, "SET %s %d siezof %d\n", name, setting, sizeof(double));
    // for(size_t i = 0; i < len; i++){
    //     mp_printf(MP_PYTHON_PRINTER, "%d ", *(uint8_t*)(value+i));
    // }
    
    if(thingz_eeprom_info.eeprom_present){
        uint8_t retry = 0;
        while(retry < 2){
            if(thingz_memory_write_array_eeprom(_thingz_memory_map_setting_to_eeprom_addr(setting), (uint8_t*)value, len) != ESP_OK){
                needFallback = 1;
            }else{
                return ESP_OK;
            }
            retry++;
        }
    }
    // mp_printf(MP_PYTHON_PRINTER, "present %d fallback %d\n", thingz_eeprom_info.eeprom_present, needFallback);
    if(needFallback){
        nvs_handle* nvsHandle;
        nvsHandle = thingz_memory_get_handle();
        if(nvs_set_blob(*nvsHandle, name, value, fallback_len) == ESP_OK){
            nvs_commit(*nvsHandle);
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

//Check current eeprom protocol version 
static int _thingz_memory_need_migration(void){
    if(thingz_eeprom_info.eeprom_present == 0 || thingz_eeprom_info.eeprom_protocol_version == thingz_eeprom_info.expected_protocol_version){
        return false;
    }
    //Protocol version is higher that what we expect, we don't know how to handle it
    if(thingz_eeprom_info.eeprom_protocol_version > thingz_eeprom_info.expected_protocol_version){
        thingz_eeprom_info.eeprom_corrupt = THINGZ_MEMORY_EEPROM_UNKNOWN_VERSION;
        return false;
    }

    return true;
}

static int _thingz_memory_migrate(void){
    nvs_handle* nvsHandle;
    nvsHandle = thingz_memory_get_handle();

    uint8_t eeprom_backup_len;
    esp_err_t err;
    err = nvs_get_u8(*nvsHandle, "erom_bkup_len", &eeprom_backup_len);
    if(err == ESP_ERR_NVS_NOT_FOUND){
        //No failed migration

        uint8_t settings[EEPROM_V0_LENGTH];
    
        for(int i = 0; i < EEPROM_V0_LENGTH; i++){
            if(thingz_memory_read_eeprom(i, settings+i) == -1){
                //failed migration, no corruption
                printf("Failed reading setting %d\n", i);
                return -1;
            }
            // printf("setting %d value %d\n", i, settings[i]);
        }

        if(nvs_set_blob(*nvsHandle, "erom_bkup", settings, EEPROM_V0_LENGTH) != ESP_OK){
            //failed migration, no corruption
            printf("Failed set nvs\n");
            return -1;
        }

        if(nvs_set_u8(*nvsHandle, "erom_bkup_len", EEPROM_V0_LENGTH) != ESP_OK){
            //failed migration, no corruption
            printf("Failed set nvs len\n");
            return -1;
        }

        nvs_commit(*nvsHandle);

        err = 0;
        for(int i=EEPROM_V1_VERSION; i < EEPROM_V1_LENGTH; i++){
            if(thingz_memory_write_eeprom(i, settings[i-EEPROM_V1_VERSION]) == -1){
                err = 1;
                printf("write setting %d failed\n", i);
                break;
            }
        }

    }else if(err == ESP_OK){
        //backup found
        uint8_t settings[eeprom_backup_len];

        for(int i = 0; i < eeprom_backup_len; i++){
            if(thingz_memory_write_eeprom(i+EEPROM_V1_VERSION, settings[i]) == -1){
                err = 1;
                printf("write setting %d failed\n", i);
                break;
            }
        }
    }else{
        //Failed migration, cannot search for previously stored backup eeprom
        return -1;
    }

    if(err == 1){
        //Eeprom is corrupt
        return -2;
    }else{
        if(thingz_memory_write_eeprom(EEPROM_V1_EEPROM_VERSION, THINGZ_MEMORY_EEPROM_EXPECTED_VERSION) == -1){
            //Eeprom is corrupt
            return -2;
        }
        thingz_eeprom_info.eeprom_protocol_version = THINGZ_MEMORY_EEPROM_EXPECTED_VERSION;
        nvs_erase_key(*nvsHandle, "erom_bkup_len");
        nvs_erase_key(*nvsHandle, "erom_bkup");

    }

    return 0;
}

uint8_t thingz_memory_is_eeprom_corrupt(void){
    if(thingz_eeprom_info.eeprom_present && thingz_eeprom_info.eeprom_corrupt){
        return thingz_eeprom_info.eeprom_corrupt;
    }
    return 0;
}

void thingz_memory_replicate_settings(void){
    uint32_t data;
    nvs_handle* nvsHandle;
    nvsHandle = thingz_memory_get_handle();
    
    // nvs_get_u32(*nvsHandle, "version", (uint32_t*)&data);
    // mp_printf(MP_PYTHON_PRINTER, "Need migration %d %d\n", thingz_memory_actual_eeprom, data);

    if(_thingz_memory_need_migration()){
        uint8_t retry = 0;
        int err;
        do{
            err = _thingz_memory_migrate();
            retry++;
        }while(err != 0 && retry < 5);

        if(err == -2){
            //Eeprom is corrupted
            thingz_eeprom_info.eeprom_corrupt = THINGZ_MEMORY_EEPROM_CORRUPT;
        }

    }
    if(nvs_get_u32(*nvsHandle, "version", (uint32_t*)&data) == ESP_OK){
        uint32_t version =0;
        
        if(thingz_memory_get_setting("version", &version) == 0){
            thingz_memory_set_setting("version", &data);
        }
    }

    if(nvs_get_u32(*nvsHandle, "display", (uint32_t*)&data) == ESP_OK){
        uint32_t display =0;
        if(thingz_memory_get_setting("display", &display) == 0 && display != data){
            thingz_memory_set_setting("display", &data);
        }
    }
    
}

int32_t thingz_memory_get_setting(const char* name, void* value){
    // mp_printf(MP_PYTHON_PRINTER, "actual %d\n", thingz_memory_actual_eeprom);
    if(thingz_eeprom_info.eeprom_corrupt){
        return -1;
    }

    // mp_printf(MP_PYTHON_PRINTER, "%s read %d\n", name, readFromEeprom);
    if(strcmp(name, "version") == 0){
        //Version used to be stored in a uint32_t inside nvs
        return _thingz_memory_get_eeprom_setting_or_fallback(EEPROM_V1_VERSION, name, value, 1, 4);
    }else if(strcmp(name, "display") == 0){
        //Display used to be stored in a uint32_t inside nvs
        return _thingz_memory_get_eeprom_setting_or_fallback(EEPROM_V1_DISPLAY, name, value, 1, 4);
    }else if(strcmp(name, "compass_hard_x") == 0){
        return _thingz_memory_get_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_HARD_X, name, value, 4, 4);
    }else if(strcmp(name, "compass_hard_y") == 0){
        return _thingz_memory_get_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_HARD_Y, name, value, 4, 4);
    }else if(strcmp(name, "compass_hard_z") == 0){
        return _thingz_memory_get_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_HARD_Z, name, value, 4, 4);
    }else if(strcmp(name, "compass_soft_x") == 0){
        return _thingz_memory_get_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_SOFT_X, name, value, 4, 4);
    }else if(strcmp(name, "compass_soft_y") == 0){
        return _thingz_memory_get_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_SOFT_Y, name, value, 4, 4);
    }else if(strcmp(name, "compass_soft_z") == 0){
        return _thingz_memory_get_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_SOFT_Z, name, value, 4, 4);
    }else if(strcmp(name, "compass_soft_m") == 0){
        float m[4];
        float (*matrix)[3] = (float (*)[3])value;

        esp_err_t err = _thingz_memory_get_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_SOFT_MATRIX, name, m, 4*4, 4*4);
        
        if(err != ESP_OK){
            return err;
        }
        for(int i = 0; i < 3; i++){
            for(int j = 0; j < 3; j++){
                if(i == j){
                    matrix[i][j] = m[i];
                }else{
                    matrix[i][j] = 0;
                }
            }
        }
        matrix[1][0] = m[3];
        return ESP_OK;
    }
    return -1;
}

int32_t thingz_memory_set_setting(const char* name, void* value){
   
    if(strcmp(name, "version") == 0){
        return _thingz_memory_set_eeprom_setting_or_fallback(EEPROM_V1_VERSION, name, value, 1, 4);
    }else if(strcmp(name, "display") == 0){
        return _thingz_memory_set_eeprom_setting_or_fallback(EEPROM_V1_DISPLAY, name, value, 1, 4);
    }else if(strcmp(name, "compass_hard_x") == 0){
        return _thingz_memory_set_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_HARD_X, name, value, 4, 4);
    }else if(strcmp(name, "compass_hard_y") == 0){
        return _thingz_memory_set_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_HARD_Y, name, value, 4, 4);
    }else if(strcmp(name, "compass_hard_z") == 0){
        return _thingz_memory_set_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_HARD_Z, name, value, 4, 4);
    }else if(strcmp(name, "compass_soft_x") == 0){
        return _thingz_memory_set_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_SOFT_X, name, value, 4, 4);
    }else if(strcmp(name, "compass_soft_y") == 0){
        return _thingz_memory_set_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_SOFT_Y, name, value, 4, 4);
    }else if(strcmp(name, "compass_soft_z") == 0){
        return _thingz_memory_set_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_SOFT_Z, name, value, 4, 4);
    }else if(strcmp(name, "compass_soft_m") == 0){
        float m[4];
        float (*matrix)[3] = (float (*)[3])value;
        for(int i = 0; i < 3; i++){
            for(int j = 0; j < 3; j++){
                if(i == j){
                    m[i] = matrix[i][j];
                }
            }
        }
        m[3] = matrix[1][0];

        return _thingz_memory_set_eeprom_setting_or_fallback(EEPROM_V1_COMPASS_SOFT_MATRIX, name, m, 4*4, 4*4);
    
    }
    return -1;
}


void thingz_memory_load_eeprom_info(void){
    thingz_eeprom_info.eeprom_corrupt = 0;
    thingz_eeprom_info.eeprom_present = 0;
    thingz_eeprom_info.expected_protocol_version = THINGZ_MEMORY_EEPROM_EXPECTED_VERSION;

    if(thingz_memory_read_eeprom(0, &thingz_eeprom_info.eeprom_protocol_version) == ESP_OK){
        thingz_eeprom_info.eeprom_present = 1;
        if(thingz_eeprom_info.eeprom_protocol_version == 0 || thingz_eeprom_info.eeprom_protocol_version == 255){
            //Byte 0 store 0 or 255 that means it is the pcb version, so it is protocol version 0
            thingz_eeprom_info.eeprom_protocol_version = 0;
        }
    }

    thingz_memory_replicate_settings();
}