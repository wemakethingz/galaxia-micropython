#include "reboot.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "shared/tinyusb/mp_usbd.h"
#include "usb.h"

void debug_mode_show_reboot_screen(void){
    mp_usbd_deinit();
    // uint64_t t = esp_timer_get_time();
    // mp_printf(MP_PYTHON_PRINTER, "reboot %lld \n", esp_timer_get_time() - t );
    // while(1){
    //     mp_printf(MP_PYTHON_PRINTER, "reboot\n");
        
    // }
    esp_restart();
}

void debug_mode_reboot_enter(void){
    
}

void debug_mode_reboot_exit(void){

}