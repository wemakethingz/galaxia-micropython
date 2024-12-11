#include "thingz_screen_repl.h"
#include "py/misc.h"
#include "thingz_screen.h"
#include "common-thingz/thingz.h"

#include "string.h"

#include "py/unicode.h"

#include "lib/tft/ili9340.h"
#include "lib/tft/fontx.h"
#include "lib/tft/font.h"

#include "mpconfigboard.h"

void thingz_screen_repl_init(thingz_screen_repl_t *repl, thingz_screen_obj_t *screen){
    repl->dataColumns = MICROPY_THINGZ_SCREEN_WIDTH/6;
    repl->dataLines = MICROPY_THINGZ_SCREEN_HEIGHT/14 -1;
    repl->virtual_top = -1;

    repl->cursor_x = 0;
    repl->cursor_y = repl->dataLines-1;
    repl->last_cursor_x = repl->cursor_x;
    repl->last_cursor_y = repl->cursor_y;

    repl->screen = screen;

}

void thingz_screen_repl_enter(){
    char* filename = thingz_get_python_file_to_exec(true);
    //thingz_print_filename(filename);
}

void thingz_screen_repl_exit(){

}

static void _thingz_screen_repl_scroll(thingz_screen_repl_t *repl){
    if(repl->cursor_y < 0){
        repl->cursor_y = 0;
        uint8_t tmp[repl->dataColumns];
        for(int j=0; j < repl->dataColumns; j++){
            repl->data[repl->dataLines-1][j] = repl->data[repl->dataLines-1][j];
        }
        for(int i = repl->dataLines-1; i > 0; i--){
            for(int j=0; j < repl->dataColumns; j++){
                repl->data[i][j] = repl->data[i-1][j];
            }
        }
        for(int j=0; j < repl->dataColumns; j++){
            repl->data[0][j] = tmp[j];
        }
    }
}

void thingz_screen_repl_refresh(thingz_screen_repl_t *repl){
    // uint8_t str[repl->dataColumns*5];
    // uint8_t index = 0;
    for(int i = 0; i < repl->dataLines; i++){
        // index = 0;
        // for(int j = 0; j < repl->dataColumns; j++){
        //     if( repl->data[i][j] > 0x7f){
        //         *((uint32_t*)str+index) = repl->data[i][j];
        //         index+=4;
        //     }else{
        //         str[index] = repl->data[i][j] ;
        //         index++;
        //     }
        // }
        thingz_screen_print_screen_with_glyp_index(repl->data[i], repl->dataColumns, 0, i>0 ? (i*repl->screen->params.font_height)-1 : (i*repl->screen->params.font_height), 0xffff); //leave 1px for the header line
    }
}

mp_uint_t thingz_screen_repl_write(thingz_screen_repl_t *repl, const void *buf, mp_uint_t size, int *errcode){
    uint8_t b[2];
    b[1] = 0;
    const byte *bu = (byte*)buf;
    const byte *i = (byte*)buf;
    while(i < bu + size){
        unichar c = utf8_get_char(i);
        i = utf8_next_char(i);
        _thingz_screen_repl_scroll(repl);
        if(repl->cursor_y != repl->last_cursor_y){
            for(int j = 0; j < repl->dataColumns; j++){
                repl->data[repl->cursor_y][j] = 0xff;
            }
        }

        if(c == '\r'){
            repl->cursor_x = 0;
        }else if(c == '\n'){
            repl->cursor_y -= 1;
        } else if (c == '\b') {
            if (repl->cursor_x > 0) {
                repl->cursor_x--;
            }
        } else if (c == 0x1b) {
            if (i < bu+size && i[0] == '[') {
                if (i+1 < bu+size && i[1] == 'K') {
                    // Clear the rest of the line.
                    for (uint16_t j = repl->cursor_x; j < repl->dataColumns; j++) {
                        repl->data[repl->cursor_y][j] = 0xff;
                    }
                    i += 2;
                } else {
                    // Handle commands of the form \x1b[####D
                    uint16_t n = 0;
                    uint8_t j = 1;
                   
                    for (; j < 6; j++) {
                        if ('0' <= i[j] && i[j] <= '9') {
                            n = n * 10 + (i[j] - '0');
                        } else {
                            c = i[j];
                            break;
                        }
                    }
                    if (c == 'D') {
                        if (n > repl->cursor_x) {
                            repl->cursor_x = 0;
                        } else {
                            repl->cursor_x -= n;
                        }
                    }
                    if (c == 'J') {
                        if (n == 2) {
                            repl->cursor_x = 0;
                            repl->cursor_y = repl->dataLines - 1;
                            repl->last_cursor_y = repl->cursor_y;
                            repl->last_cursor_x = repl->cursor_x;
                            n = 0;
                            for (uint16_t x = 0; x < repl->dataColumns; x++) {
                                for (uint16_t y = 0; y < repl->dataLines; y++) {
                                    repl->data[y][x] = 0xff;
                                }
                            }
                        }
                    }
                    if (c == ';') {
                        uint16_t m = 0;
                        for (++j; j < 9; j++) {
                            if ('0' <= i[j] && i[j] <= '9') {
                                m = m * 10 + (i[j] - '0');
                            } else {
                                c = i[j];
                                break;
                            }
                        }
                        if (c == 'H') {
                            if (n > 0) {
                                n--;
                            }
                            if (m > 0) {
                                m--;
                            }
                            if (n >= repl->dataLines) {
                                n = repl->dataLines - 1;
                            }
                            if (m >= repl->dataColumns) {
                                m = repl->dataColumns - 1;
                            }
                            
                            repl->cursor_x = m;
                            repl->cursor_y = repl->dataLines-1-n;
                        }
                    }
                    i += j + 1;
                    continue;
                }
            }
        }else{
            repl->data[repl->cursor_y][repl->cursor_x] = font_get_glyph_index(c);//0xe9;
            repl->cursor_x++;
        }

        if(repl->cursor_x >= repl->dataColumns){
            repl->cursor_x = 0;
            repl->cursor_y--;
        }

        repl->last_cursor_y = repl->cursor_y;
        repl->last_cursor_x = repl->cursor_x;
    }

    return size;
}


