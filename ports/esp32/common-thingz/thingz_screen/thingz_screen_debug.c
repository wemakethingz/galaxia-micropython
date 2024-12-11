#include "thingz_screen_debug.h"
#include "thingz_screen.h"

#include "string.h"

#include "py/unicode.h"

#include "lib/tft/ili9340.h"
#include "lib/tft/fontx.h"
#include "lib/tft/font.h"

#include "mpconfigboard.h"

void thingz_screen_debug_init(thingz_screen_debug_t *debug, thingz_screen_obj_t *screen){
    debug->dataColumns = MICROPY_THINGZ_SCREEN_WIDTH/6;
    debug->dataLines = MICROPY_THINGZ_SCREEN_HEIGHT/14;
    debug->virtual_top = -1;

    debug->cursor_x = 0;
    debug->cursor_y = debug->dataLines-1;
    debug->last_cursor_x = debug->cursor_x;
    debug->last_cursor_y = debug->cursor_y;

    debug->screen = screen;

}

void thingz_screen_debug_enter(){

}

void thingz_screen_debug_exit(){

}

// static void _thingz_screen_repl_scroll(thingz_screen_repl_t *repl){
//     if(repl->cursor_y < 0){
//         repl->cursor_y = 0;
//         uint8_t tmp[repl->dataColumns];
//         for(int j=0; j < repl->dataColumns; j++){
//             repl->data[repl->dataLines-1][j] = repl->data[repl->dataLines-1][j];
//         }
//         for(int i = repl->dataLines-1; i > 0; i--){
//             for(int j=0; j < repl->dataColumns; j++){
//                 repl->data[i][j] = repl->data[i-1][j];
//             }
//         }
//         for(int j=0; j < repl->dataColumns; j++){
//             repl->data[0][j] = tmp[j];
//         }
//     }
// }

void thingz_screen_debug_refresh(thingz_screen_debug_t *debug){

    for(int i = 0; i < debug->dataLines; i++){
        thingz_screen_print_screen_with_glyp_index(debug->data[i], debug->dataColumns, 0, ((debug->dataLines-1)*debug->screen->params.font_height)-(i*debug->screen->params.font_height), 0xffff); //leave 1px for the header line
    }
}

void thingz_screen_debug_set_str(thingz_screen_debug_t *debug, uint8_t* str, uint8_t x, uint8_t y){
    uint8_t len = strlen((char*)str);
    for(uint8_t i = x; i-x < len && i < debug->dataColumns; i++){
        debug->data[y][i] = font_get_glyph_index(str[i-x]);
    }
}