#include "thingz_memory.h"
#include "common-thingz/thingz_i2c/thingz_i2c.h"
#include "esp_err.h"
#include "py/mpprint.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#define THINGZ_MEMORY_EEPROM_ADDR 0xA0

nvs_handle nvsHandle;

void thingz_memory_init(void){
    //logger partition
    esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = "storage",
		.max_files = 16,
		.format_if_mount_failed =true
	};
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    nvs_open("nvs", NVS_READWRITE, &nvsHandle);
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			printf("Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			printf("Failed to find SPIFFS partition");
            
		} else {
			printf("Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return;
	}
    thingz_memory_load_eeprom_info();
}

nvs_handle* thingz_memory_get_handle(void){
    return &nvsHandle;
}

int32_t thingz_memory_read_eeprom(uint16_t addr, uint8_t* value){
    return common_thingz_i2c_read(THINGZ_MEMORY_EEPROM_ADDR, 0, addr , value, 1);
}

int32_t thingz_memory_write_eeprom(uint16_t addr, uint8_t value){
    return common_thingz_i2c_write(THINGZ_MEMORY_EEPROM_ADDR, 0, addr,  &value, 1);
}

int32_t thingz_memory_read_array_eeprom(uint16_t addr, uint8_t* value, uint8_t len){
    return common_thingz_i2c_read(THINGZ_MEMORY_EEPROM_ADDR, 0, addr , value, len);
}

int32_t thingz_memory_write_array_eeprom(uint16_t addr, uint8_t* value, uint8_t len){
    uint8_t l = len, s = 8 - (addr % 8); 
    int i = 0;
    

    while(i < len){
        if(s > l ){
            s = l;
        }
        esp_err_t err = common_thingz_i2c_write(THINGZ_MEMORY_EEPROM_ADDR, 0, addr+i,  value+i, s);
        vTaskDelay(pdMS_TO_TICKS(10));
        // mp_printf(MP_PYTHON_PRINTER, "Add %d, len %d, res %d\n", addr+i, s, err);
        l -= s;
        i+=s;
        s = 8;
        if(err != ESP_OK){
            return err;
        }
    }
    return ESP_OK;
}

const char* thingz_memory_get_pcb_version_name(void){
    uint32_t version = 0;
    
    if(thingz_memory_get_setting("version", &version) == -1){
        return "1.0.4";
    }

    switch(version){
        case 255:
            return "1.0.5";
        case 0:
            return "1.0.6";
        case 1:
            return "1.0.7";
        default:
            return "-";
    }
}