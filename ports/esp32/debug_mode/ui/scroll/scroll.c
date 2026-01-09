#include <string.h>

#include "scroll.h"
#include "debug_mode/debug_mode.h"

void debug_mode_ui_scroll_init(debug_mode_ui_scroll_t* scroll, uint8_t x, uint8_t y_start, uint8_t y_end, int32_t items_length, int32_t index, void* data, uint32_t data_item_length, bool dynamic_data){
    int i;
    scroll->x = x;
    scroll->y_start = y_start;
    scroll->y_end = y_end;
    scroll->items_length = items_length;
    scroll->index = index;
    scroll->data = data;
    scroll->data_item_length = data_item_length;
    scroll->active_row = -1;
    scroll->last_active_row = -1;
    scroll->need_update = false;
    scroll->dynamic_data = dynamic_data;

    scroll->text_scrolls = malloc(sizeof(debug_mode_ui_text_scroll_t)*((scroll->y_end - scroll->y_start)+1));

    for(i = 0; i < (scroll->y_end - scroll->y_start)+1 ; i++){
        // if(dynamic_data)
        //     *(((char*)(scroll->data))+scroll->data_item_length*i) = 0;
        debug_mode_ui_text_scroll_init(scroll->text_scrolls+i, x+2, scroll->y_start+i, 20, (((char*)(scroll->data))+scroll->data_item_length*i), 0, 200000);
        // debug_mode_ui_text_scroll_init(scroll->text_scrolls+i, x+2, scroll->y_start+i, 20, (char*)(scroll->data+scroll->data_item_length*i), 0, 200000);

    }
}

void debug_mode_ui_scrool_free(debug_mode_ui_scroll_t* scroll){
    free(scroll->text_scrolls);
}

int8_t debug_mode_ui_scroll_up(debug_mode_ui_scroll_t* scroll){
    if(scroll->index == 0 && scroll->active_row == scroll->y_start){
        //exit scroll zone
        return -1;
    }

    if(scroll->active_row == -1){
        if(scroll->last_active_row != -1)
            scroll->active_row = scroll->last_active_row;
        else
            scroll->active_row = scroll->y_end;
    }else{
        scroll->active_row--;
    }

    if(scroll->active_row < scroll->y_start){
        scroll->active_row = scroll->y_start;
        scroll->first_decorator_char =  !scroll->first_decorator_char;
        scroll->index--;
    }else{
        
    }        
    
    if(scroll->index < 0){
        scroll->index = 0;
    }
    
    scroll->need_update = true;
    return scroll->active_row;
}

int8_t debug_mode_ui_scroll_down(debug_mode_ui_scroll_t* scroll){
    if(scroll->items_length != 0xffff && scroll->active_row == scroll->y_end){
        //exit scroll zone
        return -1;
    }

    if(scroll->active_row == -1){
        if(scroll->last_active_row != -1)
            scroll->active_row = scroll->last_active_row;
        else
            scroll->active_row = scroll->y_start;
    }else{
        scroll->active_row++;
    }

    if(scroll->active_row  > scroll->y_end){
        scroll->active_row = scroll->y_end;
        scroll->first_decorator_char =  !scroll->first_decorator_char;
        
        scroll->index++;
        //if items_length != 0xffff that means we are at the end of the list and we kmow the exact length, so stop increasing index
        if(scroll->index + (scroll->y_end - scroll->y_start) > scroll->items_length ){
            scroll->index--;
        }
    }else if(strlen(debug_mode_ui_scroll_get_current_item(scroll)) == 0){
        //Still room to scroll to but no more element
        scroll->active_row--;
        return -1;
    }

    scroll->need_update = true;
    return scroll->active_row;
}

char* debug_mode_ui_scroll_get_current_item(debug_mode_ui_scroll_t* scroll){
    if(scroll->active_row != -1 && scroll->data){
        return (char*)(((char*)(scroll->data))+scroll->data_item_length*(scroll->active_row-scroll->y_start));
        // return (char*)(scroll->data+sizeof(char*)*(scroll->active_row-scroll->y_start));

    }
    return NULL;
}

int8_t debug_mode_ui_scroll_get_current_item_index(debug_mode_ui_scroll_t* scroll){
    return scroll->active_row;
}

void debug_mode_ui_scroll_set_current_item_index(debug_mode_ui_scroll_t* scroll, int8_t index){
    scroll->active_row = index;
}

void debug_mode_ui_scroll_print(debug_mode_ui_scroll_t* scroll){
    if(!scroll->data)
        return;
    int i;
    bool s = scroll->first_decorator_char;
    
    for(i = 0; i < (scroll->y_end - scroll->y_start)+1 ; i++){
        char str[3];

        if(!debug_mode_ui_text_scroll_get_text(scroll->text_scrolls+i) || strlen(debug_mode_ui_text_scroll_get_text(scroll->text_scrolls+i)) <= 0){
            continue;
        }
        
        if(s){
            str[0] = '|';
            str[1] = ' ';
        }else{
            str[0] = 'I';
            str[1] = ' ';
        }
        str[2] = 0;
        debug_mode_print_str(scroll->x, scroll->y_start + i, str, 0, 0xffffff);
        debug_mode_ui_text_scroll_print(scroll->text_scrolls+i);
        // debug_mode_print_str(scroll->x+2, scroll->y_start + i, (const char*)(((char*)(scroll->data))+scroll->data_item_length*i), 0, 0xffffff);
        str[1] = str[0];
        str[0] = ' ';
        debug_mode_print_str(23, scroll->y_start + i, str, 0, 0xffffff);
        s = !s;
    }
}