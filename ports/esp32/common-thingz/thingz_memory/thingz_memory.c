#include "thingz_memory.h"

#include "esp_vfs.h"
#include "esp_spiffs.h"

nvs_handle nvsHandle;

void thingz_memory_init(void){
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

    // Use settings defined above toinitialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is anall-in-one convenience function.
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
}

nvs_handle* thingz_memory_get_handle(void){
    return &nvsHandle;
}