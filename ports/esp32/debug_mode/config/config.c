#include <string.h>

#include "config.h"

// #include "supervisor/filesystem.h"
// #include "supervisor/shared/autoreload.h"

#include "lib/oofatfs/ff.h"

#include "debug_mode/debug_mode.h"
#include "debug_mode/ui/scroll/scroll.h"
#include "debug_mode/ui/selector/selector.h"
#include "debug_mode/ui/text_scroll/text_scroll.h"

#include "common-thingz/thingz.h"

#include "mphalport.h"
#include "shared/runtime/pyexec.h"

#include "py/objstr.h"

#define DEBUG_MODE_CONFIG_MIN_SELECTED_ROW 1
#define DEBUG_MODE_CONFIG_MAX_SELECTED_ROW 8

#define DEBUG_MODE_CONFIG_SCROLL_FILE_START 1
#define DEBUG_MODE_CONFIG_SCROLL_FILE_END 8

#define DEBUG_MODE_CONFIG_EVENT_UP 0
#define DEBUG_MODE_CONFIG_EVENT_DOWN 1
#define DEBUG_MODE_CONFIG_EVENT_VALIDATE 2

static int8_t selected_row;

static char *files_name;
static debug_mode_ui_scroll_t scroll_files;
static debug_mode_ui_selector_t selector;

static char config_page_title[] = "Fichier à exécuter";

static char *extended_page_title;

static void _button_a_event(thingz_input_event_t event);
static void _button_n_event(thingz_input_event_t event);
static void _button_s_event(thingz_input_event_t event);
static void _button_e_event(thingz_input_event_t event);
static void _button_w_event(thingz_input_event_t event);

static void _config_handle_event(uint8_t event);

static void input_event_cb(thingz_input_event_t event)
{
    if (event.obj == &thingz_buttons[0])
    {
        _button_a_event(event);
    }
    else if (event.obj == &thingz_buttons[1])
    {
        // _button_b_event(event);
    }
    else if (event.obj == &thingz_touch_buttons[0])
    {
        _button_n_event(event);
    }
    else if (event.obj == &thingz_touch_buttons[1])
    {
        _button_s_event(event);
    }
    else if (event.obj == &thingz_touch_buttons[2])
    {
        _button_e_event(event);
    }
    else if (event.obj == &thingz_touch_buttons[3])
    {
        _button_w_event(event);
    }
}

static void _button_a_event(thingz_input_event_t event)
{
    if (event.event == THINGZ_INPUT_EVENT_BUTTON_RELEASE)
    {
        _config_handle_event(DEBUG_MODE_CONFIG_EVENT_VALIDATE);
    }
}

static void _button_n_event(thingz_input_event_t event)
{
    if (event.event == THINGZ_INPUT_EVENT_BUTTON_PRESS)
    {
        _config_handle_event(DEBUG_MODE_CONFIG_EVENT_UP);
    }
}

static void _button_s_event(thingz_input_event_t event)
{
    if (event.event == THINGZ_INPUT_EVENT_BUTTON_PRESS)
    {
        _config_handle_event(DEBUG_MODE_CONFIG_EVENT_DOWN);
    }
}

static void _button_e_event(thingz_input_event_t event)
{
}

static void _button_w_event(thingz_input_event_t event)
{
}

static void _config_handle_event(uint8_t event)
{

    switch (event)
    {
    case DEBUG_MODE_CONFIG_EVENT_UP:
        if (selector.y >= DEBUG_MODE_CONFIG_SCROLL_FILE_START && selector.y <= DEBUG_MODE_CONFIG_SCROLL_FILE_END)
        {
            int8_t new_scroll = debug_mode_ui_scroll_up(&scroll_files);
            if (new_scroll == -1)
            {
                debug_mode_ui_selector_set_position(&selector, selector.x, DEBUG_MODE_CONFIG_SCROLL_FILE_START - 1);
            }
            else
            {
                debug_mode_ui_selector_set_position(&selector, selector.x, new_scroll);
            }
        }
        else
        {
            debug_mode_ui_selector_set_position(&selector, selector.x, selector.y - 1);
        }

        break;
    case DEBUG_MODE_CONFIG_EVENT_DOWN:
        if (selector.y >= DEBUG_MODE_CONFIG_SCROLL_FILE_START && selector.y <= DEBUG_MODE_CONFIG_SCROLL_FILE_END)
        {
            int8_t new_scroll = debug_mode_ui_scroll_down(&scroll_files);
            if (new_scroll == -1)
            {
                debug_mode_ui_selector_set_position(&selector, selector.x, DEBUG_MODE_CONFIG_SCROLL_FILE_END + 1);
            }
            else
            {
                debug_mode_ui_selector_set_position(&selector, selector.x, new_scroll);
            }
        }
        else
        {
            debug_mode_ui_selector_set_position(&selector, selector.x, selector.y + 1);
        }

        break;
    case DEBUG_MODE_CONFIG_EVENT_VALIDATE:
        if (selector.y >= DEBUG_MODE_CONFIG_SCROLL_FILE_START && selector.y <= DEBUG_MODE_CONFIG_SCROLL_FILE_END)
        {
            char *name = debug_mode_ui_scroll_get_current_item(&scroll_files);
            if (name)
            {
                thingz_set_python_file_to_exec((const char *)name);
                if(!pyexec_repl_active)
                    mp_sched_reload_interrupt();
                else{
                    ringbuf_put(&stdin_ringbuf, 4);
                }
            }
        }
        break;
    }
}

static void convert_to_utf8(char *file, char *destination, size_t dest_len)
{
    uint32_t length = strlen(file);
    // uint8_t count = 0;

    // for(uint32_t j = 0; j < length; j++){
    //     if(file[j] == 0xAE || file[j] == 0xAF || file[j] == 0x90 || file[j] == 0x85 || file[j] == 0x87
    //     || file[j] == 0x8A || file[j] == 0x82 || file[j] == 0x88 || file[j] == 0x8C || file[j] == 0x93){
    //         count++;
    //     }
    // }

    uint8_t k = 0;
    for (uint32_t j = 0; j < length && k < dest_len; j++)
    {

        if (file[j] == 0x80)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0x87;
        }
        else if (file[j] == 0x81)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xBC;
        }
        else if (file[j] == 0x82)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xA9;
        }
        else if (file[j] == 0x83)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xA2;
        }
        else if (file[j] == 0x84)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xA4;
        }
        else if (file[j] == 0x85)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xA0;
        }
        else if (file[j] == 0x86)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xA5;
        }
        else if (file[j] == 0x87)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xA7;
        }
        else if (file[j] == 0x88)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xAA;
        }
        else if (file[j] == 0x89)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xAB;
        }
        else if (file[j] == 0x8A)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xA8;
        }
        else if (file[j] == 0x8B)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xAF;
        }
        else if (file[j] == 0x8C)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xAE;
        }
        else if (file[j] == 0x8D)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xAC;
        }
        else if (file[j] == 0x8E)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0x84;
        }
        else if (file[j] == 0x8F)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0x85;
        }
        else if (file[j] == 0x90)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0x89;
        }
        else if (file[j] == 0x91)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xA6;
        }
        else if (file[j] == 0x92)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0x86;
        }
        else if (file[j] == 0x93)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xB4;
        }
        else if (file[j] == 0x94)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xB6;
        }
        else if (file[j] == 0x95)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xB2;
        }
        else if (file[j] == 0x96)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xBB;
        }
        else if (file[j] == 0x97)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xB9;
        }
        else if (file[j] == 0x98)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xBf;
        }
        else if (file[j] == 0x99)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0x96;
        }
        else if (file[j] == 0x9A)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0x9C;
        }
        else if (file[j] == 0x9B)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xA2;
        }
        else if (file[j] == 0x9C)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xA3;
        }
        else if (file[j] == 0x9D)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xA5;
        }
        else if (file[j] == 0x9E)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x82;
            k++;
            destination[k] = 0xA7;
        }
        else if (file[j] == 0x9F)
        {
            destination[k] = 0xC6;
            k++;
            destination[k] = 0x92;
        }
        else if (file[j] == 0xA0)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xA1;
        }
        else if (file[j] == 0xA1)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xAD;
        }
        else if (file[j] == 0xA2)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xB3;
        }
        else if (file[j] == 0xA3)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xBA;
        }
        else if (file[j] == 0xA4)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0xB1;
        }
        else if (file[j] == 0xA5)
        {
            destination[k] = 0xC3;
            k++;
            destination[k] = 0x91;
        }
        else if (file[j] == 0xA6)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xAA;
        }
        else if (file[j] == 0xA7)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xBA;
        }
        else if (file[j] == 0xA8)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xBF;
        }
        else if (file[j] == 0xA9)
        {
            destination[k] = 0xE2;
            k++;
            destination[k] = 0x8C;
            k++;
            destination[k] = 0x90;
        }
        else if (file[j] == 0xAA)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xAC;
        }
        else if (file[j] == 0xAB)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xBD;
        }
        else if (file[j] == 0xAC)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xBC;
        }
        else if (file[j] == 0xAD)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xA1;
        }
        else if (file[j] == 0xAE)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xAB;
        }
        else if (file[j] == 0xAF)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xBB;
        }
        else if (file[j] == 0xB0)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x96;
            k++;
            destination[k] = 0x91;
        }
        else if (file[j] == 0xB1)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x96;
            k++;
            destination[k] = 0x92;
        }
        else if (file[j] == 0xB2)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x96;
            k++;
            destination[k] = 0x93;
        }
        else if (file[j] == 0xB3)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0x82;
        }
        else if (file[j] == 0xB4)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0xa4;
        }
        else if (file[j] == 0xB5)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xa1;
        }
        else if (file[j] == 0xB6)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xa2;
        }
        else if (file[j] == 0xB7)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x96;
        }
        else if (file[j] == 0xB8)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x95;
        }
        else if (file[j] == 0xB9)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xa3;
        }
        else if (file[j] == 0xBA)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x91;
        }
        else if (file[j] == 0xBB)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x97;
        }
        else if (file[j] == 0xBC)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x9d;
        }
        else if (file[j] == 0xBD)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x9c;
        }
        else if (file[j] == 0xBE)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x9b;
        }
        else if (file[j] == 0xBF)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0x90;
        }
        else if (file[j] == 0xC0)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0x94;
        }
        else if (file[j] == 0xC1)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0xb4;
        }
        else if (file[j] == 0xC2)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0xac;
        }
        else if (file[j] == 0xC3)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0x9c;
        }
        else if (file[j] == 0xC4)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0x80;
        }
        else if (file[j] == 0xC5)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0xbc;
        }
        else if (file[j] == 0xC6)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x9e;
        }
        else if (file[j] == 0xC7)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x9f;
        }
        else if (file[j] == 0xC8)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x9a;
        }
        else if (file[j] == 0xC9)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x94;
        }
        else if (file[j] == 0xCA)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xa9;
        }
        else if (file[j] == 0xCB)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xa6;
        }
        else if (file[j] == 0xCC)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xa0;
        }
        else if (file[j] == 0xCD)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x90;
        }
        else if (file[j] == 0xCE)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xac;
        }
        else if (file[j] == 0xCF)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xa7;
        }
        else if (file[j] == 0xD0)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xa8;
        }
        else if (file[j] == 0xD1)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xa4;
        }
        else if (file[j] == 0xD2)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xa5;
        }
        else if (file[j] == 0xD3)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x99;
        }
        else if (file[j] == 0xD4)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x98;
        }
        else if (file[j] == 0xD5)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x92;
        }
        else if (file[j] == 0xD6)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0x93;
        }
        else if (file[j] == 0xD7)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xab;
        }
        else if (file[j] == 0xD8)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x95;
            k++;
            destination[k] = 0xaa;
        }
        else if (file[j] == 0xD9)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0x98;
        }
        else if (file[j] == 0xDA)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x94;
            k++;
            destination[k] = 0x8c;
        }
        else if (file[j] == 0xDB)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x96;
            k++;
            destination[k] = 0x88;
        }
        else if (file[j] == 0xDC)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x96;
            k++;
            destination[k] = 0x84;
        }
        else if (file[j] == 0xDD)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x96;
            k++;
            destination[k] = 0x8c;
        }
        else if (file[j] == 0xDE)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x96;
            k++;
            destination[k] = 0x90;
        }
        else if (file[j] == 0xDF)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x96;
            k++;
            destination[k] = 0x80;
        }
        else if (file[j] == 0xE0)
        {
            destination[k] = 0xce;
            k++;
            destination[k] = 0xb1;
        }
        else if (file[j] == 0xE1)
        {
            destination[k] = 0xc3;
            k++;
            destination[k] = 0x9f;
        }
        else if (file[j] == 0xE2)
        {
            destination[k] = 0xce;
            k++;
            destination[k] = 0x93;
        }
        else if (file[j] == 0xE3)
        {
            destination[k] = 0xcf;
            k++;
            destination[k] = 0x80;
        }
        else if (file[j] == 0xE4)
        {
            destination[k] = 0xce;
            k++;
            destination[k] = 0xa3;
        }
        else if (file[j] == 0xE5)
        {
            destination[k] = 0xcf;
            k++;
            destination[k] = 0x83;
        }
        else if (file[j] == 0xE6)
        {
            destination[k] = 0xc2;
            k++;
            destination[k] = 0xb5;
        }
        else if (file[j] == 0xE7)
        {
            destination[k] = 0xcf;
            k++;
            destination[k] = 0x84;
        }
        else if (file[j] == 0xE8)
        {
            destination[k] = 0xce;
            k++;
            destination[k] = 0xa6;
        }
        else if (file[j] == 0xE9)
        {
            destination[k] = 0xce;
            k++;
            destination[k] = 0x98;
        }
        else if (file[j] == 0xEA)
        {
            destination[k] = 0xce;
            k++;
            destination[k] = 0xa9;
        }
        else if (file[j] == 0xEB)
        {
            destination[k] = 0xce;
            k++;
            destination[k] = 0xb4;
        }
        else if (file[j] == 0xEC)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x88;
            k++;
            destination[k] = 0x9e;
        }
        else if (file[j] == 0xED)
        {
            destination[k] = 0xcf;
            k++;
            destination[k] = 0x86;
        }
        else if (file[j] == 0xEE)
        {
            destination[k] = 0xce;
            k++;
            destination[k] = 0xb5;
        }
        else if (file[j] == 0xEF)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x88;
            k++;
            destination[k] = 0xa9;
        }
        else if (file[j] == 0xF0)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x89;
            k++;
            destination[k] = 0xa1;
        }
        else if (file[j] == 0xF1)
        {
            destination[k] = 0xc2;
            k++;
            destination[k] = 0xb1;
        }
        else if (file[j] == 0xF2)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x89;
            k++;
            destination[k] = 0xa5;
        }
        else if (file[j] == 0xF3)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x89;
            k++;
            destination[k] = 0xa4;
        }
        else if (file[j] == 0xF4)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x8c;
            k++;
            destination[k] = 0xa0;
        }
        else if (file[j] == 0xF5)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x8c;
            k++;
            destination[k] = 0xa1;
        }
        else if (file[j] == 0xF6)
        {
            destination[k] = 0xc3;
            k++;
            destination[k] = 0xb7;
        }
        else if (file[j] == 0xF7)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x89;
            k++;
            destination[k] = 0x88;
        }
        else if (file[j] == 0xF8)
        {
            destination[k] = 0xc2;
            k++;
            destination[k] = 0xb0;
        }
        else if (file[j] == 0xF9)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x88;
            k++;
            destination[k] = 0x99;
        }
        else if (file[j] == 0xFA)
        {
            destination[k] = 0xc2;
            k++;
            destination[k] = 0xb7;
        }
        else if (file[j] == 0xFB)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x88;
            k++;
            destination[k] = 0x9a;
        }
        else if (file[j] == 0xFC)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x81;
            k++;
            destination[k] = 0xbf;
        }
        else if (file[j] == 0xFD)
        {
            destination[k] = 0xc2;
            k++;
            destination[k] = 0xb2;
        }
        else if (file[j] == 0xFE)
        {
            destination[k] = 0xe2;
            k++;
            destination[k] = 0x96;
            k++;
            destination[k] = 0xa0;
        }
        else if (file[j] == 0xFF)
        {
            destination[k] = 0xC2;
            k++;
            destination[k] = 0xa0;
        }
        else
        {
            destination[k] = file[j];
        }
        k++;
    }

    destination[k] = 0;
}

static int32_t _list_files(char *path, int32_t index, int32_t max, int32_t counter, uint8_t y)
{
    // mp_printf(MP_PYTHON_PRINTER, "%s\n", path);
    mp_obj_t args = {mp_obj_new_str(path, strlen(path))};
    mp_obj_list_t* list = mp_vfs_listdir(1, &args);
    // mp_printf(MP_PYTHON_PRINTER, "%d\n", list->len);
    size_t i;
    int len = strlen(path);
    for(i = 0; i < list->len; i++){
        
        GET_STR_DATA_LEN(list->items[i], s, l);
        char p[128];
        snprintf(p, 50, "%s/%s", path, s);
        
        if(strcmp((char*)s, "lib") == 0 || s[0] == '.')
            continue;
        mp_printf(MP_PYTHON_PRINTER, "%s\n", path);
        mp_obj_tuple_t* stat = mp_vfs_stat(mp_obj_new_str(p, strlen(p)));
        if(mp_obj_get_int(stat->items[0]) & 0x4000){
            //Directory
            char utf8[50];
            convert_to_utf8(s, utf8, 50);
            snprintf(&path[len], 128, "/%s", utf8);
            // mp_printf(MP_PYTHON_PRINTER, "%s\n", path);
            counter = _list_files(path, index, max, counter, y);
        }else{
            if (strncmp((char*)(s + strlen((char*)s) - 3), ".py", 3) != 0)
                continue;
            if (counter - index >= max)
                return 0xffff;
            if (counter >= index)
            {
                if (path[0] == '.' && strlen(path) == 1)
                {
                    convert_to_utf8(s, files_name + (counter - index) * 50, 49);
                }
                else
                {
                    char *str = (char *)malloc(260 * sizeof(char));
                    sprintf(str, "%s/%s", path[0] == '.' ? path + 2 : path, s);
                    convert_to_utf8(str, files_name + (counter - index) * 50, 49);
                    free(str);
                }
            }
            counter++;
        }
    }
    return counter;
}

static bool _update_files(debug_mode_ui_scroll_t *scroll)
{
    char str[128];
    sprintf(str, ".");
    scroll->items_length = _list_files(str, scroll->index, (scroll->y_end - scroll->y_start) + 1, 0, scroll->y_start);
    scroll->need_update = false;
    return true;
}

static void _active_row(void)
{
    if (selector.y >= DEBUG_MODE_CONFIG_MIN_SELECTED_ROW && selector.y <= DEBUG_MODE_CONFIG_MAX_SELECTED_ROW)
    {
        if (selector.y >= DEBUG_MODE_CONFIG_SCROLL_FILE_START && selector.y <= DEBUG_MODE_CONFIG_SCROLL_FILE_END)
        {
            debug_mode_ui_selector_set_position(&selector, selector.x, debug_mode_ui_scroll_get_current_item_index(&scroll_files));
        }
        debug_mode_ui_selector_print(&selector);
    }
}

void debug_mode_show_config_screen(void)
{
    debug_mode_clear_screen();

    debug_mode_print_header();

    if (scroll_files.need_update)
        scroll_files.update(&scroll_files);
    debug_mode_ui_scroll_print(&scroll_files);

    _active_row();
    // common_hal_displayio_display_refresh(&displays[0].display, 0xffffffff, 10000);
}

void debug_mode_config_enter(void)
{

    if (extended_page_title == NULL)
        extended_page_title = malloc(sizeof(char) * 256);

    if (files_name == NULL)
    {
        files_name = (char *)malloc(((DEBUG_MODE_CONFIG_SCROLL_FILE_END - DEBUG_MODE_CONFIG_SCROLL_FILE_START) + 1) * 50 * sizeof(char));
        size_t i;
        for (i = 0; i < (DEBUG_MODE_CONFIG_SCROLL_FILE_END - DEBUG_MODE_CONFIG_SCROLL_FILE_START) + 1; i++)
        {
            ((char *)(files_name + i * 50))[0] = '\0';
        }
    }

    selected_row = DEBUG_MODE_CONFIG_MIN_SELECTED_ROW;

    thingz_set_input_target(THINGZ_INPUT_TARGET_TGZ_DEBUG, &input_event_cb);
    debug_mode_ui_scroll_init(&scroll_files, 1, DEBUG_MODE_CONFIG_SCROLL_FILE_START, DEBUG_MODE_CONFIG_SCROLL_FILE_END, 0xffff, 0, files_name, 50, true);
    scroll_files.update = _update_files;
    scroll_files.need_update = true;
    debug_mode_ui_scroll_set_current_item_index(&scroll_files, selected_row);

    debug_mode_ui_selector_init(&selector, 0, 0, DEBUG_MODE_CONFIG_MIN_SELECTED_ROW, DEBUG_MODE_CONFIG_MAX_SELECTED_ROW, '*', 500000);

    if (strlen(thingz_get_python_file_to_exec(false)) == 0)
        thingz_set_python_file_to_exec("code.py");
    char *name = thingz_get_python_file_to_exec(false);
    sprintf(extended_page_title, "%s : %s", config_page_title, name);
    debug_mode_set_header_text(extended_page_title);
}

void debug_mode_config_exit(void)
{
    debug_mode_ui_scrool_free(&scroll_files);
    thingz_set_input_target(THINGZ_INPUT_TARGET_PYTHON, NULL);

    // for(i = 0; i < (DEBUG_MODE_CONFIG_SCROLL_FILE_END-DEBUG_MODE_CONFIG_SCROLL_FILE_START)+1; i++){
    //     free(files_name[i]);
    // }
    free(files_name);
    files_name = NULL;

    free(extended_page_title);
    extended_page_title = NULL;
}