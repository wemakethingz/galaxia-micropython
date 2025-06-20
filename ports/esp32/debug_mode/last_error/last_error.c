#include "last_error.h"

#include "debug_mode/debug_mode.h"
#include "debug_mode/ui/text_scroll/text_scroll.h"

static char last_error_page_title[] = "Dernière erreur";

static debug_mode_ui_text_scroll_t text_scroll;

void debug_mode_show_last_error_screen(void){
    debug_mode_clear_screen();
    
    debug_mode_print_header();

    // debug_mode_print_str(0,1,debug_mode_get_last_exception(), 0, 0xffffff);   
    debug_mode_ui_text_scroll_print(&text_scroll); 
    // common_hal_displayio_display_refresh(&displays[0].display, 10000, 10000);

}

void debug_mode_last_error_enter(void){
    debug_mode_set_header_text(last_error_page_title);
    char* title = debug_mode_get_last_exception();
    if(!title)
        title = "";

    debug_mode_ui_text_scroll_init(&text_scroll, 0, 1, 8*26, title, 0, 200000);
}

void debug_mode_last_error_exit(void){
    
}