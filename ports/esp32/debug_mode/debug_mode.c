#include <string.h>

#include "debug_mode.h"
#include "buttons/buttons.h"
#include "last_error/last_error.h"
#include "led/led.h"
#include "sensors/sensors.h"
#include "variables/variables.h"
#include "config/config.h"
#include "home/home.h"
#include "reboot/reboot.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

// #include "supervisor/serial.h"
// #include "supervisor/shared/display.h"
// #include "supervisor/memory.h"

// #include "shared-bindings/displayio/TileGrid.h"
// #include "shared-bindings/displayio/Palette.h"

#include "py/mpstate.h"
#include "py/obj.h"
#include "py/objint.h"
#include "py/gc.h"

#include "esp_log.h"

#include "common-thingz/thingz/thingz.h"

#include "debug_mode/ui/text_scroll/text_scroll.h"

#define DEBUG_BUTTON_GPIO 0
#define DEBUG_GPIO_INPUT_PIN_SEL  (1ULL<<DEBUG_BUTTON_GPIO) 
#define DEBUG_MIN_TIME_BTW_INTERUPT 50

#define DEBUG_BUTTON_LONG_PUSHED_TIME 1000

#define DEBUG_BUTTON_ACTION_PUSHED 1
#define DEBUG_BUTTON_ACTION_LONG_PUSHED 2
#define DEBUG_ACTION_STOP_TASK 3


static QueueHandle_t buttonQueue = NULL;
static uint64_t pushTimestamp = 0;
static bool waitLongPushedRelease = false;
static bool pushTimerFirstCall = true;

static void _debounce_timer_cb(void *args);
static esp_timer_handle_t debounceTimer;
static const esp_timer_create_args_t debounceTimerArgs = { .callback = &_debounce_timer_cb, .name = "debounce" };

// static supervisor_allocation* debug_grid_tiles = NULL;

static const char TAG[] = "DEBUG";

static uint8_t currentScreen = DEBUG_SCREEN_LOCAL_VAR;
static bool modeDebug = false;
static bool init = false;

static TaskHandle_t debugTaskHandle;

// displayio_group_t* user_group;

// static displayio_tilegrid_t debug_grid = {
//     .base = {.type = &displayio_tilegrid_type },
//     .bitmap = (displayio_bitmap_t*) &supervisor_terminal_font_bitmap,
//     .x = 0,
//     .y = 0,
//     .pixel_width = 16,
//     .pixel_height = 16,
//     .bitmap_width_in_tiles = 1,
//     .width_in_tiles = 1,
//     .height_in_tiles = 1,
//     .tile_width = 16,
//     .tile_height = 16,
//     .top_left_x = 16,
//     .top_left_y = 16,
//     .tiles = 0,
//     .partial_change = false,
//     .full_change = false,
//     .hidden = false,
//     .hidden_by_parent = false,
//     .moved = false,
//     .inline_tiles = true,
//     .in_group = true
// };

// mp_obj_t debug_group_members[] = {&debug_grid};
// mp_obj_list_t debug_group_children = {
//     .base = {.type = &mp_type_list },
//     .alloc = 1,
//     .len = 1,
//     .items = debug_group_members,
// };

// displayio_group_t debug_group = {
//     .base = {.type = &displayio_group_type },
//     .x = 0,
//     .y = 0,
//     .scale = 1,
//     .members = &debug_group_children,
//     .item_removed = false,
//     .in_group = false,
//     .hidden = false,
//     .hidden_by_parent = false
// };

// static _displayio_color_t debug_group_colors[2] = {
//     {
//         .rgb888 = 0x000000,
//         .rgb565 = 0x0000,
//         .luma = 0x00,
//         .chroma = 0
//     },
//     {
//         .rgb888 = 0xffffff,
//         .rgb565 = 0xffff,
//         .luma = 0xff,
//         .chroma = 0
//     },
//     // {
//     //     .rgb888 = 0xffff00,
//     //     .rgb565 = 0xFFE0,
//     //     .luma = 0xff,
//     //     .chroma = 0
//     // },
// };

// static displayio_palette_t debug_group_palette = {
//     .base = {.type = &displayio_palette_type },
//     .colors = debug_group_colors,
//     .color_count = 2,
//     .needs_refresh = false
// };

static char debug_last_exception[512] = {0};
static uint8_t debug_expose_exception = 0;
static char *header_text;
static debug_mode_ui_text_scroll_t header_scroll;


static void thgz_debug_exception_printer(void *env, const char *str, size_t len) {
    (void)env;
    // printf("str: %d - %s\n", len, str);
    size_t length = strlen(debug_last_exception);
    if(length < 512){
       memcpy(debug_last_exception+length, str, length+len > 512 ? 512-length : len);
    }
    // printf("str f: %d - %s\n", strlen(debug_last_exception), debug_last_exception);
}

const mp_print_t thgz_debug_exception_print = {
    NULL,
    thgz_debug_exception_printer
};

static void IRAM_ATTR button_ISR(void* arg)
{
    gpio_intr_disable(DEBUG_BUTTON_GPIO);
    esp_timer_start_periodic(debounceTimer, DEBUG_MIN_TIME_BTW_INTERUPT*1000);
    pushTimestamp = esp_timer_get_time();
    waitLongPushedRelease = false;
    pushTimerFirstCall = true;
}

static void _debounce_timer_cb(void*args){
    bool level = gpio_get_level(DEBUG_BUTTON_GPIO);
    if(level){
        //HIGH, pushed
        uint32_t buttonAction = DEBUG_BUTTON_ACTION_PUSHED;
        esp_timer_stop(debounceTimer);
        if(!waitLongPushedRelease && !pushTimerFirstCall)
            xQueueSend(buttonQueue, &buttonAction, 0);
        gpio_intr_enable(DEBUG_BUTTON_GPIO);
        waitLongPushedRelease = false;
    }else{
        //LOW
        if(esp_timer_get_time() - pushTimestamp > DEBUG_BUTTON_LONG_PUSHED_TIME*1000){
            //Long pushed
            if(waitLongPushedRelease == false){
                waitLongPushedRelease = true;
                uint32_t buttonAction = DEBUG_BUTTON_ACTION_LONG_PUSHED;
                // esp_timer_stop(debounceTimer);
                
                xQueueSend(buttonQueue, &buttonAction, 0);
            }
            
            // gpio_intr_enable(DEBUG_BUTTON_GPIO);
        }
    }
    pushTimerFirstCall = false;
}

static void _exit_screen(uint8_t current){
    switch(current){
        case DEBUG_SCREEN_CONFIG:
            debug_mode_config_exit();
        break;
        case DEBUG_SCREEN_HOME:
            debug_mode_home_exit();
        break;
        case DEBUG_SCREEN_SENSOR_BUTTONS:
            debug_mode_buttons_exit();
        break;
        case DEBUG_SCREEN_SENSOR_LED:
            debug_mode_led_exit();
        break;
        case DEBUG_SCREEN_SENSORS:
            debug_mode_sensors_exit();
        break;
        case DEBUG_SCREEN_LOCAL_VAR:
            debug_mode_local_var_exit();
        break;
        case DEBUG_SCREEN_LAST_ERROR:
            debug_mode_last_error_exit();
        break;
        case DEBUG_SCREEN_REBOOT:
            debug_mode_reboot_exit();
        break;
    }
}

static void _enter_screen(uint8_t current){
    switch(current){
        case DEBUG_SCREEN_CONFIG:
            debug_mode_config_enter();
        break;
        case DEBUG_SCREEN_HOME:
            debug_mode_home_enter();
        break;
        case DEBUG_SCREEN_SENSOR_BUTTONS:
            debug_mode_buttons_enter();
        break;
        case DEBUG_SCREEN_SENSOR_LED:
            debug_mode_led_enter();
        break;
        case DEBUG_SCREEN_SENSORS:
            debug_mode_sensors_enter();
        break;
        case DEBUG_SCREEN_LOCAL_VAR:
            debug_mode_local_var_enter();
        break;
        case DEBUG_SCREEN_LAST_ERROR:
            debug_mode_last_error_enter();
        break;
        case DEBUG_SCREEN_REBOOT:
            debug_mode_reboot_enter();
        break;
    }
}

static void _exit_debug(void);
static mp_obj_t mp_debug_handle_screen(mp_obj_t cur){
    uint8_t* current = (uint8_t*)cur;
    //When clean_vm is called we can no longer call m_malloc
    //Some debug interface may not work properly (ie accelormeter.get need to allocate a list)
    //So we exit
    if (gc_is_locked()) {
        _exit_debug();
        return mp_const_none;
    }
    thingz_screen_autorefresh(0);
    switch(*current){
        case DEBUG_SCREEN_HOME:
            debug_mode_show_home_screen();
        break;
        case DEBUG_SCREEN_SENSOR_BUTTONS:
            debug_mode_show_buttons_screen();
        break;
        case DEBUG_SCREEN_SENSOR_LED:
            debug_mode_show_led_screen();
        break;
        case DEBUG_SCREEN_SENSORS:
            debug_mode_show_sensors_screen();
        break;
        case DEBUG_SCREEN_LOCAL_VAR:
            debug_mode_show_local_var_screen();
        break;
        case DEBUG_SCREEN_LAST_ERROR:
            debug_mode_show_last_error_screen();
        break;
        case DEBUG_SCREEN_CONFIG:
            debug_mode_show_config_screen();
        break;
        case DEBUG_SCREEN_REBOOT:
            debug_mode_show_reboot_screen();
        break;
    }
    thingz_screen_autorefresh(1);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_debug_handle_screen_obj, mp_debug_handle_screen);

static void _exit_debug(void){
    currentScreen = DEBUG_SCREEN_SENSOR_BUTTONS;
    modeDebug = false;
    debug_home_reset();
    thingz_set_input_target(THINGZ_INPUT_TARGET_PYTHON, NULL);
    // int i = 0;
    // for(i = 0; i < 1; i++){
    //     debug_mode_clear_screen();
    //     // common_hal_displayio_display_refresh(&displays[0].display, 10000, 10000);
    // }
}

static void _deinit_debug_task(void){
    if(buttonQueue){
        vQueueDelete(buttonQueue);
    }

    // if(debug_grid_tiles){
    //     free(debug_grid_tiles);
    //     debug_grid_tiles = NULL;
    // }

    init = false;
    vTaskDelete(NULL);

}

static void _allocate_debug_tiles(void){
    // uint32_t total_tiles = DEBUG_SCREEN_WIDTH_IN_TILES*DEBUG_SCREEN_HEIGHT_IN_TILES;

    // if(debug_grid_tiles)
    //     return;
    // //debug_grid_tiles = allocate_memory(align32_size(total_tiles), false, true);
    
    // if (debug_grid_tiles == NULL) {
    //     debug_grid.tiles = malloc(total_tiles);
    //     // debug_grid.tiles = m_malloc(total_tiles, true);
    // } 
    // // else {
    // //     debug_grid.tiles = (uint8_t*) debug_grid_tiles->ptr;
    // // }

    // for (uint32_t i = 0; i < total_tiles; i++) {
    //     debug_grid.tiles[i] = 0;
    // }
}

static void _debug_task(void *pvParameter) {
    uint32_t buttonAction;
    uint8_t previousMode =0;

    for(;;) {
        uint32_t timeToWait = modeDebug == true ? 0 : 600000;
        buttonAction = 0;
        if(xQueueReceive(buttonQueue, &buttonAction, timeToWait / portTICK_PERIOD_MS) || modeDebug) {
            //ESP_LOGI(TAG, "debug %d action %d currentScreen %d", modeDebug, buttonAction, currentScreen);
            // mp_printf(MP_PYTHON_PRINTER, "debug %d action %d currentScreen %d", modeDebug, buttonAction, currentScreen);
            // vTaskDelay(pdMS_TO_TICKS(3000));
            switch(buttonAction){
                case DEBUG_BUTTON_ACTION_PUSHED:
                    if(!modeDebug){
                        _allocate_debug_tiles();
                        modeDebug = true;
                        previousMode = thingz_screen_get_mode();
                        thingz_screen_switch_mode(COMMON_THINGZ_SCREEN_MODE_DEBUG);
                        currentScreen = 0;
                        _enter_screen(currentScreen);
                    }else{
                        _exit_screen(currentScreen);
                        if(currentScreen == DEBUG_SCREEN_HOME){
                            _exit_debug();
                            thingz_screen_switch_mode(previousMode);
                            continue;
                        }else{
                            currentScreen = DEBUG_SCREEN_HOME;
                        }
                        _enter_screen(currentScreen);
                    }
                break;
                case DEBUG_BUTTON_ACTION_LONG_PUSHED:
                    _exit_debug();
                    continue;
                break;
                case DEBUG_ACTION_STOP_TASK:
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    if(init)
                        _deinit_debug_task();
                break;
            }
            
            if(modeDebug){
                mp_sched_schedule((mp_obj_t)&mp_debug_handle_screen_obj, &currentScreen);            
            }
            
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        // mp_printf(MP_PYTHON_PRINTER, "fewfewf\n");
        // vTaskDelay(10);
    }
    
}

void debug_mode_start(void){
    // mp_printf(MP_PYTHON_PRINTER, "START\n");
    if(init == true)return;
    // mp_printf(MP_PYTHON_PRINTER, "START 2\n");
    header_text = malloc(sizeof(char)*256);

    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = DEBUG_GPIO_INPUT_PIN_SEL; 
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en =0;    

    gpio_reset_pin(0);

    if(gpio_config(&io_conf) != ESP_OK){
        // mp_printf(MP_PYTHON_PRINTER, "FAILED BUTTON\n");
        return;
    }

    buttonQueue = xQueueCreate(10, sizeof(uint32_t));
    if(buttonQueue == NULL){
        // mp_printf(MP_PYTHON_PRINTER, "BUTTON 2\n");
        return ;
    }    

    esp_err_t status = gpio_install_isr_service(0);
    if(status != ESP_OK && status != ESP_ERR_INVALID_STATE){
        // mp_printf(MP_PYTHON_PRINTER, "FAILED ISR\n");
        return ;
    }

    if(gpio_isr_handler_add(DEBUG_BUTTON_GPIO, button_ISR, NULL) != ESP_OK){
        // mp_printf(MP_PYTHON_PRINTER, "FAILED ISR 2\n");
        return ;
    }
    
    if(esp_timer_create(&debounceTimerArgs, &debounceTimer) != ESP_OK){
        // mp_printf(MP_PYTHON_PRINTER, "FAILED TIMER\n");
        return ;
    }
    if(xTaskCreate(&_debug_task, "debug_task", 4096, NULL, 2, &debugTaskHandle) != pdPASS){
        // mp_printf(MP_PYTHON_PRINTER, "FAILED TASK\n");
        return;
    }

    debug_mode_ui_text_scroll_init(&header_scroll, 2, 0, 23, header_text, strlen(header_text), 200000);
    header_scroll.center = true;

    init = true;
}

void debug_mode_stop(void){
    // mp_printf(MP_PYTHON_PRINTER, "STOP\n");
    if(!init)
        return;

    if(modeDebug)
        _exit_debug();
    currentScreen = 0;
    
    esp_timer_stop(debounceTimer);
    esp_timer_delete(debounceTimer);

    gpio_isr_handler_remove(DEBUG_BUTTON_GPIO);
    
    uint32_t action = DEBUG_ACTION_STOP_TASK;
    xQueueSend(buttonQueue, &action, portMAX_DELAY);

    while(init == true){
        ESP_LOGI(TAG, "debug %d", init);
        // mp_printf(MP_PYTHON_PRINTER, "STOP\n");
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    if(header_text){
        free(header_text);
        header_text = NULL;
    }
}

void debug_mode_expose_last_exception(void){
    debug_expose_exception = 1;
}

char* debug_mode_get_last_exception(void){
    if(debug_expose_exception)
        return debug_last_exception;
    return NULL;
}

void debug_mode_reset_last_exception(void){
    int i;
    for(i = 0; i < 256; i++)
        debug_last_exception[i] = 0;
    debug_expose_exception = 0;
}

size_t debug_mode_utf8_strlen(const byte *b){
    uint32_t length = strlen((const char*)b);

    if(length < 2)
        return length;

    uint32_t actual_len = 0;
    const byte* i = b;
    
    while( i < (byte*)(b+length)){
        actual_len++;
        i = utf8_next_char(i);
    }
    return actual_len;
}


void debug_mode_print_str(uint8_t x, uint8_t y, const char* str, uint32_t background, uint32_t frontground){
    const byte* i = (byte*) str;
    uint32_t length = strlen(str);

    
   
    while( i < (byte*)(str+length)){
        if(y > 8 || x >= DEBUG_SCREEN_WIDTH_IN_TILES)
            return;
        unichar c = utf8_get_char(i);
        //printf("char %d\n", c);
        i = utf8_next_char(i);
        
        if (c < 128) {
            if (c >= 0x20 && c <= 0x7e) {
                // uint8_t index = fontio_builtinfont_get_glyph_index(MP_OBJ_FROM_PTR(&supervisor_terminal_font), c);
                // common_hal_displayio_tilegrid_set_tile(&debug_grid, x, y, index);
                thingz_screen_debug_set_str(&thingz_screen.debug, (uint8_t*)&c, x, y);
            }else if(c == '\n'){
                y++;
            }else if(c == '\r'){
                x=0;
                continue;
            }else if(c == '\t'){
                // uint8_t index = fontio_builtinfont_get_glyph_index(MP_OBJ_FROM_PTR(&supervisor_terminal_font), ' ');
                // common_hal_displayio_tilegrid_set_tile(&debug_grid, x, y, index);
                thingz_screen_debug_set_str(&thingz_screen.debug, (uint8_t*)" ", x, y);
            }
        }else{
            thingz_screen_debug_set_str(&thingz_screen.debug, (uint8_t*)&c, x, y);
            // uint8_t index = fontio_builtinfont_get_glyph_index(MP_OBJ_FROM_PTR(&supervisor_terminal_font), c);
            // if(index != 0xff && index < debug_grid.tiles_in_bitmap)
            //     // common_hal_displayio_tilegrid_set_tile(&debug_grid, x, y, index);
            // else
            //     // common_hal_displayio_tilegrid_set_tile(&debug_grid, x, y, fontio_builtinfont_get_glyph_index(MP_OBJ_FROM_PTR(&supervisor_terminal_font), ' '));
        }
        
        x++;
        if(x == DEBUG_SCREEN_WIDTH_IN_TILES){
            x = 0;
            y++;
        }
    }
}

// static void _clearPartialLine(uint8_t x, uint8_t y){
//     for(int i = x; i < DEBUG_SCREEN_WIDTH_IN_TILES; i++ ){
//         common_hal_displayio_tilegrid_set_tile(&debug_grid, i, y, 0);  
//     }
// }

void debug_mode_clear_screen(void){
    // debug_group_colors[0].rgb565 = 0;
    // debug_group_colors[1].rgb565 = 0xffff;
    for(int i = 0; i < DEBUG_SCREEN_WIDTH_IN_TILES; i++ ){
        for(int j = 0; j < DEBUG_SCREEN_HEIGHT_IN_TILES; j++ ){
            // common_hal_displayio_tilegrid_set_tile(&debug_grid, i, j, 0);  
            thingz_screen_debug_set_str(&thingz_screen.debug, (uint8_t*)" ", i, j);
        }
    }
}

void debug_mode_set_current_screen(uint8_t screen){
    if(modeDebug && screen < DEBUG_SCREEN_MAX){
        _exit_screen(currentScreen);
        currentScreen = screen;
        _enter_screen(currentScreen);
    }
}

void debug_mode_print_header(void){
    debug_mode_print_str(0, 0, "# ", 0, 0xffffffff);
    debug_mode_ui_text_scroll_print(&header_scroll);
    debug_mode_print_str(24, 0, " #", 0, 0xffffffff);

}

void debug_mode_set_header_text(char* title){
    debug_mode_ui_text_scroll_set_text(&header_scroll, title, strlen(title));
}

void debug_mode_need_exit(void){
    // if(modeDebug == false && user_group != NULL){
    //     // common_hal_displayio_display_show(&displays[0].display, user_group);
    //     user_group = NULL;
    // }
}