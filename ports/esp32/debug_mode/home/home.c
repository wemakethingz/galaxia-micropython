#include <string.h>

#include "home.h"

#include "debug_mode/debug_mode.h"
#include "debug_mode/ui/scroll/scroll.h"
#include "debug_mode/ui/selector/selector.h"

#include "common-thingz/thingz/thingz.h"


#define DEBUG_MODE_HOME_MIN_SELECTED_ROW 1
#define DEBUG_MODE_HOME_MAX_SELECTED_ROW 8

#define DEBUG_MODE_HOME_SCROLL_PAGE_START 1
#define DEBUG_MODE_HOME_SCROLL_PAGE_END 8

#define DEBUG_MODE_HOME_EVENT_UP 0
#define DEBUG_MODE_HOME_EVENT_DOWN 1
#define DEBUG_MODE_HOME_EVENT_VALIDATE 2

#define DEBUG_MODE_HOME_NB_PAGES 8

#define DEBUG_MODE_HOME_EVENT_UP 0
#define DEBUG_MODE_HOME_EVENT_DOWN 1
#define DEBUG_MODE_HOME_EVENT_VALIDATE 2


static int8_t selected_row = DEBUG_MODE_HOME_MIN_SELECTED_ROW;
static bool save_selector_position = false;

static char pages_name[DEBUG_MODE_HOME_NB_PAGES][50] = {
    "Dernière erreur",
    "Fichier à exécuter",
    "Variables",
    "Boutons",
    "LED",
    "Capteurs",
    "Redémarrer",
    ""
};

static uint8_t pages_index[DEBUG_MODE_HOME_NB_PAGES] = {
    DEBUG_SCREEN_LAST_ERROR,
    DEBUG_SCREEN_CONFIG,
    DEBUG_SCREEN_LOCAL_VAR,
    DEBUG_SCREEN_SENSOR_BUTTONS,
    DEBUG_SCREEN_SENSOR_LED,
    DEBUG_SCREEN_SENSORS,
    DEBUG_SCREEN_REBOOT,
    DEBUG_SCREEN_MAX,
};

static char home_title[] = "Menu système";

static debug_mode_ui_scroll_t scroll_pages;
static debug_mode_ui_selector_t selector;

static void _button_a_event(thingz_input_event_t event);
static void _button_b_event(thingz_input_event_t event);
static void _button_n_event(thingz_input_event_t event);
static void _button_s_event(thingz_input_event_t event);
static void _button_e_event(thingz_input_event_t event);
static void _button_w_event(thingz_input_event_t event);

static void _config_handle_event(uint8_t event);

static void input_event_cb(thingz_input_event_t event){
    if(event.obj == &thingz_buttons[0]){
        _button_a_event(event);
    }else if(event.obj == &thingz_buttons[1]){
        _button_b_event(event);
    }else if(event.obj == &thingz_touch_buttons[0]){
        _button_n_event(event);
    }else if(event.obj == &thingz_touch_buttons[1]){
        _button_s_event(event);
    }else if(event.obj == &thingz_touch_buttons[2]){
        _button_e_event(event);
    }else if(event.obj == &thingz_touch_buttons[3]){
        _button_w_event(event);
    }
}

static void _button_a_event(thingz_input_event_t event){
    if(event.event == THINGZ_INPUT_EVENT_BUTTON_RELEASE){
        _config_handle_event(DEBUG_MODE_HOME_EVENT_VALIDATE);
    }
}

static void _button_b_event(thingz_input_event_t event){

}


static void _button_n_event(thingz_input_event_t event){
    if(event.event == THINGZ_INPUT_EVENT_BUTTON_PRESS){
        _config_handle_event(DEBUG_MODE_HOME_EVENT_UP);
    }
}

static void _button_s_event(thingz_input_event_t event){
    if(event.event == THINGZ_INPUT_EVENT_BUTTON_PRESS){
        _config_handle_event(DEBUG_MODE_HOME_EVENT_DOWN);
    }
}

static void _button_e_event(thingz_input_event_t event){

}

static void _button_w_event(thingz_input_event_t event){

}

static uint8_t _find_index_by_page_name(char* name){
    int i;
    for(i = 0; i < DEBUG_MODE_HOME_NB_PAGES; i++){
        if(strcmp(name, pages_name[i]) == 0)
            return pages_index[i];
    }
    return DEBUG_SCREEN_MAX;
}

static void _config_handle_event(uint8_t event){
    
    switch(event){
        case DEBUG_MODE_HOME_EVENT_UP:
            if(selector.y >= DEBUG_MODE_HOME_SCROLL_PAGE_START && selector.y <= DEBUG_MODE_HOME_SCROLL_PAGE_END){
                int8_t new_scroll = debug_mode_ui_scroll_up(&scroll_pages);
                if(new_scroll == -1){
                    debug_mode_ui_selector_set_position(&selector, selector.x, DEBUG_MODE_HOME_SCROLL_PAGE_START -1);
                }else{
                    debug_mode_ui_selector_set_position(&selector, selector.x, new_scroll);
                }
            }else{
                debug_mode_ui_selector_set_position(&selector, selector.x, selector.y-1);

            }
           
        break;
        case DEBUG_MODE_HOME_EVENT_DOWN:
            if(selector.y >= DEBUG_MODE_HOME_SCROLL_PAGE_START && selector.y <= DEBUG_MODE_HOME_SCROLL_PAGE_END){
                int8_t new_scroll = debug_mode_ui_scroll_down(&scroll_pages);
                if(new_scroll == -1){
                    debug_mode_ui_selector_set_position(&selector, selector.x, DEBUG_MODE_HOME_SCROLL_PAGE_END+1);
                }else{
                    debug_mode_ui_selector_set_position(&selector, selector.x, new_scroll);
                }
            }else{
                debug_mode_ui_selector_set_position(&selector, selector.x, selector.y + 1);
            }
           
        break;
        case DEBUG_MODE_HOME_EVENT_VALIDATE:
            if(selector.y >= DEBUG_MODE_HOME_SCROLL_PAGE_START && selector.y <= DEBUG_MODE_HOME_SCROLL_PAGE_END){
                char *name = debug_mode_ui_scroll_get_current_item(&scroll_pages);
                if(name){
                    save_selector_position = true;
                    debug_mode_set_current_screen(_find_index_by_page_name(name));
                }
            }
        break;
    }
}

static void _active_row(void){
    if(selector.y >= DEBUG_MODE_HOME_MIN_SELECTED_ROW && selector.y <= DEBUG_MODE_HOME_MAX_SELECTED_ROW){
        if(selector.y >= DEBUG_MODE_HOME_SCROLL_PAGE_START && selector.y <= DEBUG_MODE_HOME_SCROLL_PAGE_END){
            debug_mode_ui_selector_set_position(&selector, selector.x, debug_mode_ui_scroll_get_current_item_index(&scroll_pages));
        }
        debug_mode_ui_selector_print(&selector);
    }
}

void debug_mode_show_home_screen(void){
    debug_mode_clear_screen();
    debug_mode_print_header(); 

    debug_mode_ui_scroll_print(&scroll_pages);

    _active_row();
    // common_hal_displayio_display_refresh(&displays[0].display, 0xffffffff, 10000);
}

void debug_mode_home_enter(void){

    thingz_set_input_target(THINGZ_INPUT_TARGET_TGZ_DEBUG, &input_event_cb);
    debug_mode_ui_scroll_init(&scroll_pages, 1, DEBUG_MODE_HOME_SCROLL_PAGE_START, DEBUG_MODE_HOME_SCROLL_PAGE_END, 0xffff, 0, pages_name, 50, false);

    debug_mode_ui_scroll_set_current_item_index(&scroll_pages, selected_row);

    debug_mode_ui_selector_init(&selector, 0, 0, DEBUG_MODE_HOME_MIN_SELECTED_ROW, DEBUG_MODE_HOME_MAX_SELECTED_ROW, '*', 500000);
    debug_mode_ui_selector_set_position(&selector, selector.x, selected_row);

    debug_mode_set_header_text(home_title);

    debug_mode_clear_screen();
    debug_mode_print_header(); 

}

void debug_mode_home_exit(void){
    if(save_selector_position)
        selected_row = selector.y;
    else
        selected_row = DEBUG_MODE_HOME_MIN_SELECTED_ROW;
    
    save_selector_position = false;

    debug_mode_ui_scrool_free(&scroll_pages);
    thingz_set_input_target(THINGZ_INPUT_TARGET_PYTHON, NULL);
}

void debug_home_reset(void){
    selected_row = DEBUG_MODE_HOME_MIN_SELECTED_ROW;
}