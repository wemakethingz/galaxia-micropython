#include "common-thingz/thingz_screen/thingz_screen_raw.h"
#include "freertos/projdefs.h"
#include "lib/oofatfs/ff.h"
#include "py/mpprint.h"
#include "py/obj.h"
#include "thingz_screen_repl.h"
#include "thingz_screen.h"
#include "common-thingz/thingz.h"
#include "common-thingz/thingz_display/thingz_display_raw_image.h"
#include "common-thingz/thingz_display/thingz_display_raw_rectangle.h"
#include "common-thingz/thingz_display/thingz_display_raw_text.h"


#include "string.h"

#include "py/unicode.h"
#include "py/misc.h"
#include "py/gc.h"

#include "lib/tft/ili9340.h"
#include "lib/tft/fontx.h"
#include "lib/tft/font.h"
#include "lib/tft/bmpfile.h"

#include "mpconfigboard.h"

#define THINGZ_SCREEN_RAW_TRANSFER_ACTION_ADD 0
#define THINGZ_SCREEN_RAW_TRANSFER_ACTION_REMOVE 1

typedef struct {
    uint8_t action;
    thingz_display_raw_img_obj_t* img;
} thingz_screen_raw_transfer_t;

static uint32_t _thingz_screen_read_word(uint16_t *bmp_header, uint16_t index) {
    return bmp_header[index] | bmp_header[index + 1] << 16;
}

// void common_hal_displayio_ondiskbitmap_construct(displayio_ondiskbitmap_t *self, pyb_file_obj_t *file) {
//     // Load the wave
//     thingz_screen_bitmap_t bitmap;
//     uint16_t bmp_header[69];
//     uint8_t bytes_read;

//     mp_stream_read_exactly(fp, &bmp_header, 138, &err);
//     if (err != 0) {
//         mp_raise_OSError(MP_EIO);
//     }
//     if (memcmp(bmp_header, "BM", 2) != 0) {
//         mp_raise_ValueError(translate("Invalid BMP file"));
//     }

//     // We can't cast because we're not aligned.
//     bitmap.data_offset = _thingz_screen_read_word(bmp_header, 5);

//     uint32_t header_size = _thingz_screen_read_word(bmp_header, 7);
//     uint16_t bits_per_pixel = bmp_header[14];
//     uint32_t compression = _thingz_screen_read_word(bmp_header, 15);
//     uint32_t number_of_colors = _thingz_screen_read_word(bmp_header, 23);

//     bool indexed = bits_per_pixel <= 8;
//     bitmap.bitfield_compressed = (compression == 3);
//     bitmap.bits_per_pixel = bits_per_pixel;
//     bitmap.width = _thingz_screen_read_word(bmp_header, 9);
//     bitmap.height = _thingz_screen_read_word(bmp_header, 11);


//     if (bits_per_pixel == 16) {
//         if (((header_size >= 56)) || (bitmap.bitfield_compressed)) {
//             bitmap.r_bitmask = _thingz_screen_read_word(bmp_header, 27);
//             bitmap.g_bitmask = _thingz_screen_read_word(bmp_header, 29);
//             bitmap.b_bitmask = _thingz_screen_read_word(bmp_header, 31);

//         } else { // no compression or short header means 5:5:5
//             bitmap.r_bitmask = 0x7c00;
//             bitmap.g_bitmask = 0x3e0;
//             bitmap.b_bitmask = 0x1f;
//         }
//     } else if (indexed) {
//         if (number_of_colors == 0) {
//             number_of_colors = 1 << bits_per_pixel;
//         }

//         if (number_of_colors > 1) {
//             uint16_t palette_size = number_of_colors * sizeof(uint32_t);
//             uint16_t palette_offset = 0xe + header_size;

//             uint32_t *palette_data = m_malloc(palette_size, false);

//             f_rewind(&self->file->fp);
//             f_lseek(&self->file->fp, palette_offset);

//             UINT palette_bytes_read;
//             if (f_read(&self->file->fp, palette_data, palette_size, &palette_bytes_read) != FR_OK) {
//                 mp_raise_OSError(MP_EIO);
//             }
//             if (palette_bytes_read != palette_size) {
//                 mp_raise_ValueError("Unable to read color palette data");
//             }
//         } else {
//             common_hal_displayio_palette_set_color(palette, 0, 0x0);
//             common_hal_displayio_palette_set_color(palette, 1, 0xffffff);
//         }
//         self->palette = palette;

//     } else if (!(header_size == 12 || header_size == 40 || header_size == 108 || header_size == 124)) {
//         mp_raise_ValueError_varg(translate("Only Windows format, uncompressed BMP supported: given header size is %d"), header_size);
//     }

//     if (bits_per_pixel == 8 && number_of_colors == 0) {
//         mp_raise_ValueError_varg(translate("Only monochrome, indexed 4bpp or 8bpp, and 16bpp or greater BMPs supported: %d bpp given"), bits_per_pixel);
//     }

//     uint8_t bytes_per_pixel = (self->bits_per_pixel / 8)  ? (self->bits_per_pixel / 8) : 1;
//     uint8_t pixels_per_byte = 8 / self->bits_per_pixel;
//     if (pixels_per_byte == 0) {
//         self->stride = (self->width * bytes_per_pixel);
//         // Rows are word aligned.
//         if (self->stride % 4 != 0) {
//             self->stride += 4 - self->stride % 4;
//         }
//     } else {
//         uint32_t bit_stride = self->width * self->bits_per_pixel;
//         if (bit_stride % 32 != 0) {
//             bit_stride += 32 - bit_stride % 32;
//         }
//         self->stride = (bit_stride / 8);
//     }

// }


uint32_t _thingz_get_pixel(thingz_screen_bitmap_t *bitmap,
    int16_t x, int16_t y) {
    if (x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height) {
        return 0;
    }

    uint32_t location;
    uint8_t bytes_per_pixel = (bitmap->bits_per_pixel / 8)  ? (bitmap->bits_per_pixel / 8) : 1;
    uint8_t pixels_per_byte = 8 / bitmap->bits_per_pixel;
    if (pixels_per_byte == 0) {
        location = bitmap->data_offset + (bitmap->height - y - 1) * bitmap->stride + x * bytes_per_pixel;
    } else {
        location = bitmap->data_offset + (bitmap->height - y - 1) * bitmap->stride + x / pixels_per_byte;
    }
    // We don't cache here because the underlying FS caches sectors.
    f_lseek(&bitmap->file->fp, location);
    UINT bytes_read;
    uint32_t pixel_data = 0;
    uint32_t result = f_read(&bitmap->file->fp, &pixel_data, bytes_per_pixel, &bytes_read);
    if (result == FR_OK) {
        uint32_t tmp = 0;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        if (bytes_per_pixel == 1) {
            uint8_t offset = (x % pixels_per_byte) * bitmap->bits_per_pixel;
            uint8_t mask = (1 << bitmap->bits_per_pixel) - 1;

            return (pixel_data >> ((8 - bitmap->bits_per_pixel) - offset)) & mask;
        } else if (bytes_per_pixel == 2) {
            if (bitmap->g_bitmask == 0x07e0) { // 565
                red = ((pixel_data & bitmap->r_bitmask) >> 11);
                green = ((pixel_data & bitmap->g_bitmask) >> 5);
                blue = ((pixel_data & bitmap->b_bitmask) >> 0);
            } else { // 555
                red = ((pixel_data & bitmap->r_bitmask) >> 10);
                green = ((pixel_data & bitmap->g_bitmask) >> 4);
                blue = ((pixel_data & bitmap->b_bitmask) >> 0);
            }
            tmp = (red << 19 | green << 10 | blue << 3);
            return tmp;
        } else if ((bytes_per_pixel == 4) && (bitmap->bitfield_compressed)) {
            return pixel_data & 0x00FFFFFF;
        } else {
            return pixel_data;
        }
    }
    return 0;
}

uint32_t _thingz_get_pixels(thingz_screen_bitmap_t *bitmap,
    int16_t x, int16_t y, int16_t x2, int16_t y2, uint16_t* pixels) {
    if (x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height
    ||  x2 < 0 || x2 >= bitmap->width || y2 < 0 || y2 >= bitmap->height) {
        return 0;
    }

    uint32_t location;
    uint8_t bytes_per_pixel = (bitmap->bits_per_pixel / 8)  ? (bitmap->bits_per_pixel / 8) : 1;
    uint8_t pixels_per_byte = 8 / bitmap->bits_per_pixel;

    for(uint8_t i = 0; i < (y2-y)+1; i++){
        if (pixels_per_byte == 0) {
            location = bitmap->data_offset + (bitmap->height - (y+i) - 1) * bitmap->stride + x * bytes_per_pixel;
        } else {
            location = bitmap->data_offset + (bitmap->height - (y+i) - 1) * bitmap->stride + x / pixels_per_byte;
        }
        // We don't cache here because the underlying FS caches sectors.
        f_lseek(&bitmap->file->fp, location);
        for(uint8_t j = 0; j < (x2-x)+1; j++){
            UINT bytes_read;
            uint32_t pixel_data = 0;
            uint32_t pixel;
            uint32_t result = f_read(&bitmap->file->fp, &pixel_data, bytes_per_pixel, &bytes_read);

            if (result == FR_OK) {
                uint32_t tmp = 0;
                uint8_t red;
                uint8_t green;
                uint8_t blue;
                if (bytes_per_pixel == 1) {
                    uint8_t offset = (x % pixels_per_byte) * bitmap->bits_per_pixel;
                    uint8_t mask = (1 << bitmap->bits_per_pixel) - 1;

                    pixel = (pixel_data >> ((8 - bitmap->bits_per_pixel) - offset)) & mask;
                } else if (bytes_per_pixel == 2) {
                    if (bitmap->g_bitmask == 0x07e0) { // 565
                        red = ((pixel_data & bitmap->r_bitmask) >> 11);
                        green = ((pixel_data & bitmap->g_bitmask) >> 5);
                        blue = ((pixel_data & bitmap->b_bitmask) >> 0);
                    } else { // 555
                        red = ((pixel_data & bitmap->r_bitmask) >> 10);
                        green = ((pixel_data & bitmap->g_bitmask) >> 4);
                        blue = ((pixel_data & bitmap->b_bitmask) >> 0);
                    }
                    pixel = (red << 19 | green << 10 | blue << 3);
                } else if ((bytes_per_pixel == 4) && (bitmap->bitfield_compressed)) {
                    pixel = pixel_data & 0x00FFFFFF;
                } else {
                    pixel = pixel_data;
                }

                if(bitmap->palette != NULL){
                    pixel = bitmap->palette[pixel];
                }
                if(pixel == 0xFFFFFF){
                    pixel = bitmap->white_replacement_color;
                }
                pixels[i*((x2-x)+1)+j] = rgb565_conv((pixel>>16)&0xFF, (pixel>>8)&0xFF, pixel&0xFF);
            }
        }
        
    }
    
    return 0;
}

void _thingz_screen_raw_add_show_obj_to_list(thingz_screen_raw_t* raw, mp_obj_t obj){
    thingz_screen_raw_show_obj_t* o = raw->head;
    // mp_printf(MP_PYTHON_PRINTER,"Adding %p \n", obj);

    if(o == NULL){
        thingz_screen_raw_show_obj_t* o2 = (thingz_screen_raw_show_obj_t*)malloc(sizeof(thingz_screen_raw_show_obj_t));
        o2->show_obj = obj;
        o2->next = NULL;
        o2->prev = NULL;
        raw->head = o2;
    }else{
        while(o->next != NULL){
            o = o->next;
        }
        thingz_screen_raw_show_obj_t* o2 = (thingz_screen_raw_show_obj_t*)malloc(sizeof(thingz_screen_raw_show_obj_t));
        o2->show_obj = obj;
        o2->next = NULL;
        o2->prev = o;
        o->next = o2;
    }    
}

void _thingz_screen_raw_remove_show_obj_from_list(thingz_screen_raw_t* raw, mp_obj_t obj){
    thingz_screen_raw_show_obj_t* o = raw->head;

    if(o != NULL){

        if(obj != NULL){
            while(o->show_obj != obj && o->next == NULL){
                o = o->next;
            }
            if(o->show_obj == obj){
                thingz_screen_raw_show_obj_t* next = o->next;
                thingz_screen_raw_show_obj_t* prev = o->prev;

                if(prev){
                    prev->next = next;
                }else{
                    raw->head = NULL;
                }
                free(o);
            }
        }else{
            // mp_printf(MP_PYTHON_PRINTER, "Removing\n");
            //Remove all
            thingz_screen_raw_show_obj_t* o = raw->head;

            while(o != NULL){
                thingz_screen_raw_show_obj_t* next = o->next;
                // mp_printf(MP_PYTHON_PRINTER,"Removing %p \n", o);
                free(o);
                o = next;
            }
            raw->head = NULL;
        }
    }
}


void thingz_screen_raw_init(thingz_screen_raw_t *raw, thingz_screen_obj_t *screen){
    // repl->dataColumns = MICROPY_THINGZ_SCREEN_WIDTH/6;
    // repl->dataLines = MICROPY_THINGZ_SCREEN_HEIGHT/14 -1;
    // repl->virtual_top = -1;

    // repl->cursor_x = 0;
    // repl->cursor_y = repl->dataLines-1;
    // repl->last_cursor_x = repl->cursor_x;
    // repl->last_cursor_y = repl->cursor_y;

    raw->screen = screen;
    raw->head = NULL;

}

void thingz_screen_raw_enter(thingz_screen_raw_t *raw){
    // char* filename = thingz_get_python_file_to_exec(true);
    //thingz_print_filename(filename);
}

void thingz_screen_raw_exit(thingz_screen_raw_t *raw){
    thingz_screen_raw_show_obj_t* o = raw->head;

    while(o != NULL){
        thingz_screen_raw_show_obj_t* next = o->next;
        thingz_display_raw_img_obj_t* img = o->show_obj;
        // free(o);
        img->screen_show = 0;
        o = next;
    }
}

static uint8_t _thingz_screen_raw_refresh_image(thingz_screen_raw_t *raw, thingz_display_raw_img_obj_t* img, uint8_t force_refresh){
    uint8_t printed = 0;
    if(img->show){
        if(img->screen_show == 0){
            thingz_screen_raw_print_bmp(&(thingz_screen.raw), img->x, img->y, img->path, img->white_replacement_color, 1);
            printed = 1;
        }else{
            uint8_t rect_s, rect_e, need_refresh = force_refresh;
            if(img->screen_x != img->x){
                need_refresh = 1;
                if(img->x > img->screen_x){
                    rect_s = img->screen_x;
                    rect_e = (img->x < img->screen_x + img->bmp.width ? img->x : img->screen_x+img->bmp.width-1); 
                }else{
                    rect_s = (img->x + img->bmp.width < img->screen_x ? img->screen_x : img->x + img->bmp.width);
                    rect_e = (img->screen_x+img->bmp.width-1);
                }
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), rect_s, rect_e, img->screen_y, img->screen_y+img->bmp.height-1, 0);

            }
            if(img->screen_y != img->y){
                need_refresh = 1;
                if(img->y > img->screen_y){
                    rect_s = img->screen_y;
                    rect_e = (img->y < img->screen_y + img->bmp.height ? img->y : img->screen_y+img->bmp.height-1); 
                }else{
                    rect_s = (img->y + img->bmp.height < img->screen_y ? img->screen_y : img->y + img->bmp.height);
                    rect_e = (img->screen_y+img->bmp.height-1);
                }
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), img->screen_x, img->screen_x+img->bmp.width-1, rect_s, rect_e, 0);
            }
            if(need_refresh){
                printed = 1;
                thingz_screen_raw_print_bmp(&(thingz_screen.raw), img->x, img->y, img->path, img->white_replacement_color, 1);
            }
        }
        img->screen_x = img->x;
        img->screen_y = img->y;
        img->screen_show = 1;
    }else{
        if(img->screen_show){
            thingz_screen_raw_fill_rect(&(thingz_screen.raw), img->x, img->x+img->bmp.width-1, img->y, img->y+img->bmp.height-1, 0);
            img->screen_show = 0;
        }
    }
    return printed;
}

static uint8_t _thingz_screen_raw_refresh_rectangle(thingz_screen_raw_t *raw, thingz_display_raw_rectangle_obj_t* rectangle, uint8_t force_refresh){
    uint8_t printed = 0;
    if(rectangle->show){
        if(rectangle->screen_show == 0){
            thingz_screen_raw_fill_rect(&(thingz_screen.raw), rectangle->x, rectangle->x+rectangle->width-1, rectangle->y, rectangle->y+rectangle->height-1, rgb565_conv((rectangle->color>>16)&0xFF, (rectangle->color>>8)&0xFF, rectangle->color&0xFF));
            printed = 1;
        }else{
            uint8_t rect_s, rect_e, need_refresh = force_refresh;
            if(rectangle->screen_x != rectangle->x){
                need_refresh = 1;
                if(rectangle->x > rectangle->screen_x){
                    rect_s = rectangle->screen_x;
                    rect_e = (rectangle->x < rectangle->screen_x + rectangle->screen_width ? rectangle->x : rectangle->screen_x+rectangle->screen_width-1); 
                }else{
                    rect_s = (rectangle->x + rectangle->width < rectangle->screen_x ? rectangle->screen_x : rectangle->x + rectangle->width);
                    rect_e = (rectangle->screen_x+rectangle->width-1);
                }
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), rect_s, rect_e, rectangle->screen_y, rectangle->screen_y+rectangle->height-1, 0);

            }
            if(rectangle->screen_y != rectangle->y){
                need_refresh = 1;
                if(rectangle->y > rectangle->screen_y){
                    rect_s = rectangle->screen_y;
                    rect_e = (rectangle->y < rectangle->screen_y + rectangle->screen_height ? rectangle->y : rectangle->screen_y+rectangle->screen_height-1); 
                }else{
                    rect_s = (rectangle->y + rectangle->height < rectangle->screen_y ? rectangle->screen_y : rectangle->y + rectangle->height);
                    rect_e = (rectangle->screen_y+rectangle->height-1);
                }
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), rectangle->screen_x, rectangle->screen_x+rectangle->width-1, rect_s, rect_e, 0);
            }
            if(rectangle->screen_width > rectangle->width){
                need_refresh = 1;
                rect_s = rectangle->width-1;
                rect_e = rectangle->screen_width;
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), rect_s, rect_e, rectangle->screen_y, rectangle->screen_y, 0);
            }
            if(rectangle->screen_height > rectangle->height){
                need_refresh = 1;
                rect_s = rectangle->height-1;
                rect_e = rectangle->screen_height;
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), rectangle->screen_x, rectangle->screen_x, rect_s, rect_e, 0);
            }
            if(rectangle->color != rectangle->screen_color){
                need_refresh = 1;
            }
            if(need_refresh){
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), rectangle->x, rectangle->x+rectangle->width-1, rectangle->y, rectangle->y+rectangle->height-1, rgb565_conv((rectangle->color>>16)&0xFF, (rectangle->color>>8)&0xFF, rectangle->color&0xFF));
                printed = 1;
            }
        }
        rectangle->screen_x = rectangle->x;
        rectangle->screen_y = rectangle->y;
        rectangle->screen_height = rectangle->height;
        rectangle->screen_width = rectangle->width;
        rectangle->screen_color = rectangle->color;
        rectangle->screen_show = 1;
    }else{
        if(rectangle->screen_show){
            thingz_screen_raw_fill_rect(&(thingz_screen.raw), rectangle->x, rectangle->x+rectangle->width-1, rectangle->y, rectangle->y+rectangle->height-1, 0);
            rectangle->screen_show = 0;
        }
    }
    return printed;
}

static uint8_t _thingz_screen_raw_refresh_text(thingz_screen_raw_t *raw, thingz_display_raw_text_obj_t* text, uint8_t force_refresh){
    uint8_t printed = 0;
    uint8_t width, height;
    uint16_t len = strlen(text->text);

    if(text->has_changed || text->screen_show == 0){
        height = raw->screen->params.font_height;
        width = raw->screen->params.font_width*len;
    }else{
        height = text->screen_height;
        width = text->screen_width;
    }
    if(text->show){

        if(text->screen_show == 0){

            if(len == 0)
                return printed;
            thingz_screen_raw_write(raw, text->x, text->y, text->text, len, text->color);
            printed = 1;
        }else{
            uint8_t rect_s, rect_e, need_refresh = force_refresh;

            if(text->has_changed){
                need_refresh = 1;
            }

            if(text->screen_x != text->x){
                need_refresh = 1;
                if(text->x > text->screen_x){
                    rect_s = text->screen_x;
                    rect_e = (text->x < text->screen_x + width ? text->x : text->screen_x+width-1); 
                }else{
                    rect_s = (text->x + width < text->screen_x ? text->screen_x : text->x + width);
                    rect_e = (text->screen_x+width-1);
                }
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), rect_s, rect_e, text->screen_y, text->screen_y+height-1, 0);

            }
            if(text->screen_y != text->y){
                need_refresh = 1;
                if(text->y > text->screen_y){
                    rect_s = text->screen_y;
                    rect_e = (text->y < text->screen_y + height ? text->y : text->screen_y+height-1); 
                }else{
                    rect_s = (text->y + height < text->screen_y ? text->screen_y : text->y + height);
                    rect_e = (text->screen_y+height-1);
                }
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), text->screen_x, text->screen_x+width-1, rect_s, rect_e, 0);
            }
            if(text->screen_width > width){
                need_refresh = 1;
                rect_s = width-1;
                rect_e = text->screen_width;
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), rect_s, rect_e, text->screen_y, text->screen_y, 0);
            }
            if(text->screen_height > height){
                need_refresh = 1;
                rect_s = height-1;
                rect_e = text->screen_height;
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), text->screen_x, text->screen_x, rect_s, rect_e, 0);
            }
            if(text->color != text->screen_color){
                need_refresh = 1;
            }
            if(need_refresh){
                printed = 1;
                thingz_screen_raw_write(raw, text->x, text->y, text->text, len, text->color);
            }
        }
        text->screen_x = text->x;
        text->screen_y = text->y;
        text->screen_height = height;
        text->screen_width = width;
        text->screen_color = text->color;
        text->has_changed = 0;
        text->screen_show = 1;
    }else{
        if(text->screen_show){
            thingz_screen_raw_fill_rect(&(thingz_screen.raw), text->x, text->x+width-1, text->y, text->y+height-1, 0);
            text->screen_show = 0;
        }
    }
    return printed;
}

static uint8_t _thingz_screen_raw_is_overlapping(thingz_screen_raw_show_obj_t* o1, thingz_screen_raw_show_obj_t* o2){
    uint8_t o1_x=0, o1_y=0, o1_width=0, o1_height=0, o1_sx=0, o1_sy=0, o1_swidth=0, o1_sheight=0;
    uint8_t o2_x=0, o2_y=0, o2_width=0, o2_height=0, o2_sx=0, o2_sy=0, o2_swidth=0, o2_sheight=0;

    if(mp_obj_is_type(o1->show_obj, &mp_thingz_display_raw_rectangle_type)){
        thingz_display_raw_rectangle_obj_t* rect = o1->show_obj;
        o1_x = rect->x;
        o1_y = rect->y;
        o1_width = rect->width;
        o1_height = rect->height;
        o1_sx = rect->screen_x;
        o1_sy = rect->screen_y;
        o1_swidth = rect->screen_width;
        o1_sheight = rect->screen_height;
    }else if(mp_obj_is_type(o1->show_obj, &mp_thingz_display_raw_img_type)){
        thingz_display_raw_img_obj_t* img = o1->show_obj;
        o1_x = img->x;
        o1_y = img->y;
        o1_width = img->bmp.width;
        o1_height = img->bmp.height;
        o1_sx = img->screen_x;
        o1_sy = img->screen_y;
        o1_swidth = img->bmp.width;
        o1_sheight = img->bmp.height;
    }else if(mp_obj_is_type(o1->show_obj, &mp_thingz_display_raw_text_type)){
        thingz_display_raw_text_obj_t* text = o1->show_obj;
        o1_x = text->x;
        o1_y = text->y;
        o1_width = text->screen_width;
        o1_height = text->screen_height;
        o1_sx = text->screen_x;
        o1_sy = text->screen_y;
        o1_swidth = text->screen_width;
        o1_sheight = text->screen_height;
    }
    if(mp_obj_is_type(o2->show_obj, &mp_thingz_display_raw_rectangle_type)){
        thingz_display_raw_rectangle_obj_t* rect = o2->show_obj;
        o2_x = rect->x;
        o2_y = rect->y;
        o2_width = rect->width;
        o2_height = rect->height;
        o2_sx = rect->screen_x;
        o2_sy = rect->screen_y;
        o2_swidth = rect->screen_width;
        o2_sheight = rect->screen_height;
    }else if(mp_obj_is_type(o2->show_obj, &mp_thingz_display_raw_img_type)){
        thingz_display_raw_img_obj_t* img = o2->show_obj;
        o2_x = img->x;
        o2_y = img->y;
        o2_width = img->bmp.width;
        o2_height = img->bmp.height;
        o2_sx = img->screen_x;
        o2_sy = img->screen_y;
        o2_swidth = img->bmp.width;
        o2_sheight = img->bmp.height;
    }else if(mp_obj_is_type(o1->show_obj, &mp_thingz_display_raw_text_type)){
        thingz_display_raw_text_obj_t* text = o1->show_obj;
        o2_x = text->x;
        o2_y = text->y;
        o2_width = text->screen_width;
        o2_height = text->screen_height;
        o2_sx = text->screen_x;
        o2_sy = text->screen_y;
        o2_swidth = text->screen_width;
        o2_sheight = text->screen_height;
    }

    return ((o1_sx <= o2_sx && o2_sx <= o1_sx+o1_swidth-1)
    ||     (o1_sx <= o2_sx+o2_swidth-1 && o2_sx+o2_swidth-1 <= o1_sx+o1_swidth-1)
    ||     (o1_sy <= o2_sy && o2_sy <= o1_sy+o1_sheight-1)
    ||     (o1_sy <= o2_sy+o2_sheight-1 && o2_sy+o2_sheight-1 <= o1_sy+o1_sheight-1))
    &&(    //if there is no change of object o2 we don't need to refresh o1
           (o2_sx != o2_x)
    ||     (o2_sy != o2_y)
    ||     (o2_swidth != o2_width)
    ||     (o2_sheight != o2_height)    
    ||     (o2->was_updated)
    );
}

static uint8_t _thingz_screen_raw_is_obj_on_top(thingz_screen_raw_show_obj_t* current){
    thingz_screen_raw_show_obj_t* obj = current->prev;
    uint8_t on_top = 0;
    while(obj != NULL){
        if(_thingz_screen_raw_is_overlapping(current, obj)){
            return 1;
        }
        obj = obj->next;
    }
    return on_top;

}

static uint8_t _thingz_screen_raw_is_obj_under(thingz_screen_raw_show_obj_t* current){
    thingz_screen_raw_show_obj_t* obj = current->next;
    uint8_t on_top = 0;
    while(obj != NULL){
        if(_thingz_screen_raw_is_overlapping(current, obj)){
            return 1;
        }
        obj = obj->next;
    }
    return on_top;

}


static mp_obj_t mp_thingz_screen_raw_refresh(void* r){
    //When clean_vm is called we can no longer call m_malloc
    //Some debug interface may not work properly (ie accelormeter.get need to allocate a list)
    //So we exit
    thingz_screen_raw_t* raw = r;
    if (gc_is_locked() || raw->screen->current_mode != COMMON_THINGZ_SCREEN_MODE_RAW) {
        return mp_const_none;
    }
    thingz_screen_raw_show_obj_t* obj = raw->head;
    while(obj != NULL){
        obj->was_updated = 0;
        uint8_t force_refresh = _thingz_screen_raw_is_obj_on_top(obj);
        if(!force_refresh){
            force_refresh = _thingz_screen_raw_is_obj_under(obj);
        }
        if(mp_obj_is_type(obj->show_obj, &mp_thingz_display_raw_rectangle_type)){
            obj->was_updated = _thingz_screen_raw_refresh_rectangle(raw, obj->show_obj, force_refresh);
        }else if(mp_obj_is_type(obj->show_obj, &mp_thingz_display_raw_img_type)){
            obj->was_updated = _thingz_screen_raw_refresh_image(raw, obj->show_obj, force_refresh);
        }else if(mp_obj_is_type(obj->show_obj, &mp_thingz_display_raw_text_type)){
            obj->was_updated = _thingz_screen_raw_refresh_text(raw, obj->show_obj, force_refresh);
        }
        obj = obj->next;
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_screen_raw_refresh_obj, mp_thingz_screen_raw_refresh);


void thingz_screen_raw_refresh(thingz_screen_raw_t *raw){
    // thingz_screen_raw_transfer_t transfer;
    // if( xQueueReceive( raw->transfer_queue,&( transfer ),0) == pdPASS ){
    //     if(transfer.action == THINGZ_SCREEN_RAW_TRANSFER_ACTION_ADD){
    //         _thingz_screen_raw_add_show_obj_to_list(raw, transfer.img);
    //     }else{
    //         _thingz_screen_raw_remove_show_obj_from_list(raw, transfer.img);
    //     }
    // }
    // if (raw->screen->current_mode != COMMON_THINGZ_SCREEN_MODE_RAW) {
    //     return;
    // }
    mp_sched_schedule((mp_obj_t)&mp_thingz_screen_raw_refresh_obj, raw);
}

mp_uint_t thingz_screen_raw_write(thingz_screen_raw_t *raw, uint8_t x, uint8_t y, const void *buf, mp_uint_t size, uint32_t color){
    uint8_t rotation = 1;

    thingz_screen_print_screen(buf, size, x, MICROPY_THINGZ_SCREEN_HEIGHT-1-y-raw->screen->params.font_height+1, rgb565_conv((color>>16)&0xFF, (color>>8)&0xFF, color&0xFF), 1);
    return size;
}

thingz_screen_bitmap_t thingz_screen_raw_print_bmp(thingz_screen_raw_t *raw, uint8_t x, uint8_t y, const char *file, uint32_t white_replacement_color, uint8_t show){
    thingz_screen_bitmap_t bitmap;
    bitmap.shown = 0;
    mp_obj_t args[] = {
    mp_obj_new_str(file, strlen(file)),
    mp_obj_new_str("rb", 1),
    };
    uint16_t bmp_header[69];
    pyb_file_obj_t* f = mp_vfs_open(2, args, &mp_const_empty_map);
    bitmap.file = f;
    
    f_rewind(&f->fp);
    UINT bytes_read;
    if (f_read(&f->fp, bmp_header, 138, &bytes_read) != FR_OK) {
        mp_raise_OSError(MP_EIO);
    }
    if (bytes_read != 138 ||
        memcmp(bmp_header, "BM", 2) != 0) {
        mp_raise_ValueError("Invalid BMP file");
    }
    
    // We can't cast because we're not aligned.
    bitmap.data_offset = _thingz_screen_read_word(bmp_header, 5);

    uint32_t header_size = _thingz_screen_read_word(bmp_header, 7);
    uint16_t bits_per_pixel = bmp_header[14];
    uint32_t compression = _thingz_screen_read_word(bmp_header, 15);
    uint32_t number_of_colors = _thingz_screen_read_word(bmp_header, 23);

    bool indexed = bits_per_pixel <= 8;
    bitmap.white_replacement_color = white_replacement_color;
    bitmap.bitfield_compressed = (compression == 3);
    bitmap.bits_per_pixel = bits_per_pixel;
    bitmap.width = _thingz_screen_read_word(bmp_header, 9);
    bitmap.height = _thingz_screen_read_word(bmp_header, 11);
    bitmap.palette = NULL;
    if (bits_per_pixel == 16) {
        if (((header_size >= 56)) || (bitmap.bitfield_compressed)) {
            bitmap.r_bitmask = _thingz_screen_read_word(bmp_header, 27);
            bitmap.g_bitmask = _thingz_screen_read_word(bmp_header, 29);
            bitmap.b_bitmask = _thingz_screen_read_word(bmp_header, 31);

        } else { // no compression or short header means 5:5:5
            bitmap.r_bitmask = 0x7c00;
            bitmap.g_bitmask = 0x3e0;
            bitmap.b_bitmask = 0x1f;
        }
    } else if (indexed) {
        if (number_of_colors == 0) {
            number_of_colors = 1 << bits_per_pixel;
        }
        
        uint32_t *palette_data = NULL;
        if (number_of_colors > 1) {
            uint16_t palette_size = number_of_colors * sizeof(uint32_t);
            uint16_t palette_offset = 0xe + header_size;

            palette_data = m_malloc(palette_size);

            f_rewind(&f->fp);
            f_lseek(&f->fp, palette_offset);

            UINT palette_bytes_read;
            if (f_read(&f->fp, palette_data, palette_size, &palette_bytes_read) != FR_OK) {
                f_close(&f->fp);
                m_free(palette_data);
                mp_raise_OSError(MP_EIO);
            }
            if (palette_bytes_read != palette_size) {
                f_close(&f->fp);
                m_free(palette_data);
                mp_raise_ValueError("Unable to read color palette data");
            }
        } else {
            palette_data = m_malloc(2);
            palette_data[0] = 0;
            palette_data[1] = 0xffffff;
        }
        bitmap.palette = palette_data;

    } else if (!(header_size == 12 || header_size == 40 || header_size == 108 || header_size == 124)) {
        f_close(&f->fp);
        mp_raise_ValueError("Only Windows format, uncompressed BMP supported");
    }

    if (bits_per_pixel == 8 && number_of_colors == 0) {
        f_close(&f->fp);
        if(bitmap.palette)
            m_free(bitmap.palette);
        mp_raise_ValueError("Only monochrome, indexed 4bpp or 8bpp, and 16bpp or greater BMPs supported");
    }

    if(show){
        uint8_t bytes_per_pixel = (bitmap.bits_per_pixel / 8)  ? (bitmap.bits_per_pixel / 8) : 1;
        uint8_t pixels_per_byte = 8 / bitmap.bits_per_pixel;
        if (pixels_per_byte == 0) {
            bitmap.stride = (bitmap.width * bytes_per_pixel);
            // Rows are word aligned.
            if (bitmap.stride % 4 != 0) {
                bitmap.stride += 4 - bitmap.stride % 4;
            }
        } else {
            uint32_t bit_stride = bitmap.width * bitmap.bits_per_pixel;
            if (bit_stride % 32 != 0) {
                bit_stride += 32 - bit_stride % 32;
            }
            bitmap.stride = (bit_stride / 8);
        }
        bool rotation = true;
        uint16_t *colors = (uint16_t*)m_malloc(sizeof(uint16_t) * (rotation ? bitmap.height : bitmap.width));
        uint8_t _x = 0, _x2, _y, _y2;
        for (int row=0; row< (rotation ? bitmap.width : bitmap.height); row++) { // For each scanline...
            
            // int index = 0;
            _x = rotation ? row : 0;
            _x2 = rotation ? row : bitmap.width-1;
            _y = rotation ? 0 : row;
            _y2 = rotation ? bitmap.height-1 : row;
            _thingz_get_pixels(&bitmap, _x, _y, _x2, _y2, colors);
            
            if(rotation){
                uint16_t color;
                for(int i = 0; i < bitmap.height/2; i++){
                    color = colors[i];
                    colors[i] = colors[bitmap.height-1-i];
                    colors[bitmap.height-1-i] = color;
                }
            }

            // for(int col=0; col < (rotation ? bitmap.height : bitmap.width); col++){
            //     _x = rotation ? row : col;
            //     _y = rotation ? col : row;
            //     uint32_t pixel = _thingz_get_pixel(&bitmap, _x, _y);
            //     if(bitmap.palette != NULL){
            //         pixel = bitmap.palette[pixel];
            //     }
            //     colors[col] = rgb565_conv((pixel>>16)&0xFF, (pixel>>8)&0xFF, pixel&0xFF);

            // }
            
            // ESP_LOGD(__FUNCTION__,"lcdDrawMultiPixels row=%d",row);
            lcdDrawMultiPixels(&(thingz_screen.dev), rotation ? MICROPY_THINGZ_SCREEN_HEIGHT-bitmap.height-y : x, rotation ? x+row : y+row, rotation ? bitmap.height : bitmap.width, colors);
            // lcdDrawMultiPixels(&(thingz_screen.dev), 0, row, rotation ? bitmap.height : bitmap.width, colors);
            bitmap.shown = 1;
            _x++;
        } // end for row

        m_free(colors);
    }

    f_close(&f->fp);
    if(bitmap.palette)
        m_free(bitmap.palette);
    

    return bitmap;
// //     // read bmp header
// 	bmpfile_t *result = NULL;
//     result = (bmpfile_t*)m_malloc(sizeof(bmpfile_t));
//     int err;
//     mp_stream_read_exactly(fp, result->header.magic, 2, &err);
//     // mp_printf(MP_PYTHON_PRINTER, "rt %d\n", ret);
// 	// if (result->header.magic[0]!='B' || result->header.magic[1] != 'M') {
// 	// 	free(result);
// 	// 	mp_stream_close(fp);
// 	// 	return 0;
// 	// }
//     mp_stream_read_exactly(fp, &result->header.filesz, 4, &err);
//     mp_stream_read_exactly(fp, &result->header.creator1, 2, &err);
//     mp_stream_read_exactly(fp, &result->header.creator2, 2, &err);
//     mp_stream_read_exactly(fp, &result->header.offset, 4, &err);

// 	// read dib header
//     mp_stream_read_exactly(fp, &result->dib.header_sz, 4, &err);
//     mp_stream_read_exactly(fp, &result->dib.width, 4, &err);
// 	mp_stream_read_exactly(fp, &result->dib.height, 4, &err);
// 	mp_stream_read_exactly(fp, &result->dib.nplanes, 2, &err);
// 	mp_stream_read_exactly(fp, &result->dib.depth, 2, &err);
// 	mp_stream_read_exactly(fp, &result->dib.compress_type, 4, &err);
// 	mp_stream_read_exactly(fp, &result->dib.bmp_bytesz, 4, &err);
// 	mp_stream_read_exactly(fp, &result->dib.hres, 4, &err);
// 	mp_stream_read_exactly(fp, &result->dib.vres, 4, &err);
// 	mp_stream_read_exactly(fp, &result->dib.ncolors, 4, &err);
// 	mp_stream_read_exactly(fp, &result->dib.nimpcolors, 4, &err);

//     mp_printf(MP_PYTHON_PRINTER, "fp %d %d\n", result->dib.depth, result->dib.compress_type);

// 	if( (result->dib.compress_type == 0)) {
// 		// BMP rows are padded (if needed) to 4-byte boundary
// 		uint32_t rowSize = (result->dib.width * 3 + 3) & ~3;
// 		int w = result->dib.width;
// 		int h = result->dib.height;
// 		// ESP_LOGD(__FUNCTION__,"w=%d h=%d", w, h);
// 		int _x;
// 		int _w;
// 		int _cols;
// 		int _cole;
// 		if (MICROPY_THINGZ_SCREEN_WIDTH >= w) {
// 			_x = (MICROPY_THINGZ_SCREEN_WIDTH - w) / 2;
// 			_w = w;
// 			_cols = 0;
// 			_cole = w - 1;
// 		} else {
// 			_x = 0;
// 			_w = MICROPY_THINGZ_SCREEN_WIDTH;
// 			_cols = (w - MICROPY_THINGZ_SCREEN_WIDTH) / 2;
// 			_cole = _cols + MICROPY_THINGZ_SCREEN_WIDTH - 1;
// 		}
// 		// ESP_LOGD(__FUNCTION__,"_x=%d _w=%d _cols=%d _cole=%d",_x, _w, _cols, _cole);

// 		int _y;
// 		int _rows;
// 		int _rowe;
// 		if (MICROPY_THINGZ_SCREEN_HEIGHT >= h) {
// 			_y = (MICROPY_THINGZ_SCREEN_HEIGHT - h) / 2;
// 			_rows = 0;
// 			_rowe = h -1;
// 		} else {
// 			_y = 0;
// 			_rows = (h - MICROPY_THINGZ_SCREEN_HEIGHT) / 2;
// 			_rowe = _rows + MICROPY_THINGZ_SCREEN_HEIGHT - 1;
// 		}
// 		// ESP_LOGD(__FUNCTION__,"_y=%d _rows=%d _rowe=%d", _y, _rows, _rowe);

// #define BUFFPIXEL 20
// 		uint8_t sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
// 		uint16_t *colors = (uint16_t*)m_malloc(sizeof(uint16_t) * w);

// 		for (int row=0; row<h; row++) { // For each scanline...
// 			if (row < _rows || row > _rowe) continue;
// 			// Seek to start of scan line.	It might seem labor-
// 			// intensive to be doing this on every line, but this
// 			// method covers a lot of gritty details like cropping
// 			// and scanline padding.	Also, the seek only takes
// 			// place if the file position actually needs to change
// 			// (avoids a lot of cluster math in SD library).
// 			// Bitmap is stored bottom-to-top order (normal BMP)
// 			int pos = result->header.offset + (h - 1 - row) * rowSize;
// 			mp_stream_posix_lseek(fp, pos, SEEK_SET);
// 			int buffidx = sizeof(sdbuffer); // Force buffer reload

// 			int index = 0;
// 			for (int col=0; col<w; col++) { // For each pixel...
// 				if (buffidx >= sizeof(sdbuffer)) { // Indeed
//                     mp_stream_read_exactly(fp, sdbuffer, sizeof(sdbuffer), &err);
// 					buffidx = 0; // Set index to beginning
// 				}
// 				if (col < _cols || col > _cole) continue;
// 				// Convert pixel from BMP to TFT format, push to display
// 				uint8_t b = sdbuffer[buffidx++];
// 				uint8_t g = sdbuffer[buffidx++];
// 				uint8_t r = sdbuffer[buffidx++];
// 				colors[index++] = rgb565_conv(r, g, b);
// 			} // end for col
// 			// ESP_LOGD(__FUNCTION__,"lcdDrawMultiPixels row=%d",row);
// 			lcdDrawMultiPixels(&(thingz_screen.dev), _x, _y, _w, colors);
// 			_y++;
// 		} // end for row
// 		m_free(colors);
// 	} // end if 
//     mp_stream_close(fp);
// 	m_free(result);

//     return 0;
}

void thingz_screen_raw_fill_rect(thingz_screen_raw_t* raw, uint8_t x, uint8_t x2, uint8_t y, uint8_t y2, uint16_t color){
    bool rotation = true;
    lcdDrawFillRect(&(thingz_screen.dev), rotation ? MICROPY_THINGZ_SCREEN_HEIGHT-1-y2 :x , rotation ? x : y, rotation ? MICROPY_THINGZ_SCREEN_HEIGHT-1-y : x2, rotation ? x2 : y2, color);

    // lcdDrawFillRect(&(thingz_screen.dev), rotation ? MICROPY_THINGZ_SCREEN_HEIGHT-1-y : x, rotation ? x : y, rotation ? MICROPY_THINGZ_SCREEN_HEIGHT-1-y2 : x2, rotation ? x2: y2, color);
}

void thingz_screen_raw_add_show_obj(thingz_screen_raw_t* raw, mp_obj_t obj){
    // thingz_screen_raw_transfer_t transfer;
    // transfer.action = THINGZ_SCREEN_RAW_TRANSFER_ACTION_ADD;
    // transfer.img = obj;
    // xQueueSend(raw->transfer_queue, &transfer, pdMS_TO_TICKS(1000));
    _thingz_screen_raw_add_show_obj_to_list(raw, obj);
}

void thingz_screen_raw_remove_show_obj(thingz_screen_raw_t* raw, mp_obj_t obj){
    // thingz_screen_raw_transfer_t transfer;
    // transfer.action = THINGZ_SCREEN_RAW_TRANSFER_ACTION_REMOVE;
    // transfer.img = obj;
    // xQueueSend(raw->transfer_queue, &transfer, pdMS_TO_TICKS(1000));
    _thingz_screen_raw_remove_show_obj_from_list(raw, obj);
}
