#include <string.h>

#include "text_scroll.h"

#include "esp_timer.h"

#include "debug_mode/debug_mode.h"

void debug_mode_ui_text_scroll_init(debug_mode_ui_text_scroll_t* scroll, int8_t x, int8_t y, int8_t visible_length, char* text, uint32_t length, int64_t scroll_period){
    scroll->x = x;
    scroll->y = y;
    scroll->visible_length = visible_length;
    scroll->text = text;
    scroll->length = strlen(scroll->text);
    scroll->current_index = 0;
    scroll->scroll_period = scroll_period;
    scroll->scroll_timestamp = 0;
    scroll->center = false;
}

char* debug_mode_ui_text_scroll_get_text(debug_mode_ui_text_scroll_t* scroll){
    return scroll->text;
}

void debug_mode_ui_text_scroll_set_text(debug_mode_ui_text_scroll_t* scroll, char* text, uint32_t length){
    scroll->text = text;
    scroll->length = strlen(scroll->text);
    scroll->current_index = 0;
}

void debug_mode_ui_text_scroll_set_position(debug_mode_ui_text_scroll_t* scroll, int8_t x, int8_t y){
    scroll->x = x;
    scroll->y = y;
    scroll->current_index = 0;
}

void debug_mode_ui_text_scroll_print(debug_mode_ui_text_scroll_t* scroll){

    size_t length = debug_mode_utf8_strlen((const byte*)scroll->text);
    if((int)length <= scroll->visible_length){
        if(scroll->center)
            debug_mode_print_str(scroll->x+(scroll->visible_length-length)/2, scroll->y, scroll->text, 0, 0xffffff);
        else
            debug_mode_print_str(scroll->x, scroll->y, scroll->text, 0, 0xffffff);
        scroll->current_index = 0;
    }else{
        int64_t timestamp = esp_timer_get_time();

        if(scroll->current_index > length){
            scroll->current_index = 0;
        }
        
        if(scroll->current_index+scroll->visible_length < length){
            //String larger than visible text, need to cut
            char end = scroll->text[scroll->current_index+scroll->visible_length];
            scroll->text[scroll->current_index+scroll->visible_length] = 0;
            debug_mode_print_str(scroll->x, scroll->y, scroll->text+scroll->current_index, 0, 0xffffff);
            scroll->text[scroll->current_index+scroll->visible_length] = end;
        }else{
            //remaining string smaller than visible, try to circle the string
            int sublength = debug_mode_utf8_strlen((const byte*)scroll->text+scroll->current_index);
            int circular_index_end = scroll->visible_length - sublength - 1;
            if(circular_index_end > 1){
                char end = scroll->text[circular_index_end];
                scroll->text[circular_index_end] = 0;
                debug_mode_print_str(scroll->x+sublength+1, scroll->y, scroll->text, 0, 0xffffff);
                scroll->text[circular_index_end] = end;
            } 
            debug_mode_print_str(scroll->x, scroll->y, scroll->text+scroll->current_index, 0, 0xffffff);
        }
        
        

        if(timestamp - scroll->scroll_timestamp > scroll->scroll_period){
            unichar c = utf8_get_char((const byte*)&scroll->text[scroll->current_index]);
            if(c < 128){
                scroll->current_index++;
            }else{
                scroll->current_index += 2;
            }
            scroll->scroll_timestamp = timestamp;
        }
    }
}