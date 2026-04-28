#include "common-thingz/thingz_screen/thingz_screen_raw.h"
#include "freertos/projdefs.h"
#include "lib/oofatfs/ff.h"
#include "py/mpprint.h"
#include "py/obj.h"
#include "thingz_screen_repl.h"
#include "thingz_screen.h"
#include "esp_log.h"
#include "common-thingz/thingz/thingz.h"
#include "common-thingz/thingz_display/Raw/thingz_display_raw_image.h"
#include "common-thingz/thingz_display/Raw/thingz_display_raw_rectangle.h"
#include "common-thingz/thingz_display/Raw/thingz_display_raw_text.h"


#include "string.h"

#include "py/unicode.h"
#include "py/misc.h"
#include "py/gc.h"
#include "py/mpthread.h"
#include "py/runtime.h"
#include "py/mpstate.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "lib/tft/ili9340.h"
#include "lib/tft/fontx.h"
#include "lib/tft/font.h"
#include "lib/tft/bmpfile.h"

#include "mpconfigboard.h"

#define THINGZ_SCREEN_RAW_TRANSFER_ACTION_ADD 0
#define THINGZ_SCREEN_RAW_TRANSFER_ACTION_REMOVE 1

// Helper macro to convert RGB888 color to RGB565
#define RGB888_TO_RGB565(color) rgb565_conv(((color)>>16)&0xFF, ((color)>>8)&0xFF, (color)&0xFF)

typedef struct {
    uint8_t action;
    thingz_display_raw_img_obj_t* img;
} thingz_screen_raw_transfer_t;

static uint32_t _thingz_screen_read_word(uint16_t *bmp_header, uint16_t index) {
    return bmp_header[index] | bmp_header[index + 1] << 16;
}

// Helper function to decode pixel data based on bits per pixel
static inline uint32_t _thingz_decode_pixel(uint32_t pixel_data, uint8_t bytes_per_pixel, uint8_t pixels_per_byte,
                                             uint8_t bits_per_pixel, int16_t x, thingz_screen_bitmap_t *bitmap) {
    if (bytes_per_pixel == 1) {
        uint8_t offset = (x % pixels_per_byte) * bits_per_pixel;
        uint8_t mask = (1 << bits_per_pixel) - 1;
        return (pixel_data >> ((8 - bits_per_pixel) - offset)) & mask;
    }
    if (bytes_per_pixel == 2) {
        uint8_t red, green, blue;
        if (bitmap->g_bitmask == 0x07e0) { // 565
            red = ((pixel_data & bitmap->r_bitmask) >> 11);
            green = ((pixel_data & bitmap->g_bitmask) >> 5);
            blue = ((pixel_data & bitmap->b_bitmask) >> 0);
        } else { // 555
            red = ((pixel_data & bitmap->r_bitmask) >> 10);
            green = ((pixel_data & bitmap->g_bitmask) >> 4);
            blue = ((pixel_data & bitmap->b_bitmask) >> 0);
        }
        return (red << 19 | green << 10 | blue << 3);
    }
    if ((bytes_per_pixel == 4) && (bitmap->bitfield_compressed)) {
        return pixel_data & 0x00FFFFFF;
    }
    return pixel_data;
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


uint32_t _thingz_get_pixel(thingz_screen_bitmap_t *bitmap, int16_t x, int16_t y) {
    if (x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height) {
        return 0;
    }

    uint8_t bytes_per_pixel = (bitmap->bits_per_pixel / 8) ? (bitmap->bits_per_pixel / 8) : 1;
    uint8_t pixels_per_byte = 8 / bitmap->bits_per_pixel;

    uint32_t location = bitmap->data_offset + (bitmap->height - y - 1) * bitmap->stride;
    location += (pixels_per_byte == 0) ? x * bytes_per_pixel : x / pixels_per_byte;

    f_lseek(&bitmap->file->fp, location);
    UINT bytes_read;
    uint32_t pixel_data = 0;
    if (f_read(&bitmap->file->fp, &pixel_data, bytes_per_pixel, &bytes_read) == FR_OK) {
        return _thingz_decode_pixel(pixel_data, bytes_per_pixel, pixels_per_byte, bitmap->bits_per_pixel, x, bitmap);
    }
    return 0;
}

uint32_t _thingz_get_pixels(thingz_screen_bitmap_t *bitmap,
    int16_t x, int16_t y, int16_t x2, int16_t y2, uint16_t* pixels, uint8_t* line_buffer) {
    if (x < 0 || x >= bitmap->width || y < 0 || y >= bitmap->height
    ||  x2 < 0 || x2 >= bitmap->width || y2 < 0 || y2 >= bitmap->height) {
        return 0;
    }

    uint8_t bytes_per_pixel = (bitmap->bits_per_pixel / 8) ? (bitmap->bits_per_pixel / 8) : 1;
    uint8_t pixels_per_byte = 8 / bitmap->bits_per_pixel;
    uint8_t width = (x2 - x) + 1;

    // Use pre-allocated buffer passed from caller to avoid malloc/free per line
    uint16_t line_size = width * bytes_per_pixel;

    for(uint8_t i = 0; i <= (y2 - y); i++){
        uint32_t location = bitmap->data_offset + (bitmap->height - (y + i) - 1) * bitmap->stride;
        location += (pixels_per_byte == 0) ? x * bytes_per_pixel : x / pixels_per_byte;

        // Seek once per line instead of once per pixel
        f_lseek(&bitmap->file->fp, location);

        // Read entire line in one operation
        UINT bytes_read;
        if (f_read(&bitmap->file->fp, line_buffer, line_size, &bytes_read) == FR_OK && bytes_read == line_size) {
            // Process all pixels from the buffer
            for(uint8_t j = 0; j <= (x2 - x); j++){
                uint32_t pixel_data = 0;

                // Extract pixel data from buffer
                for(uint8_t b = 0; b < bytes_per_pixel; b++){
                    pixel_data |= ((uint32_t)line_buffer[j * bytes_per_pixel + b]) << (b * 8);
                }

                uint32_t pixel = _thingz_decode_pixel(pixel_data, bytes_per_pixel, pixels_per_byte, bitmap->bits_per_pixel, x + j, bitmap);

                if(bitmap->palette != NULL){
                    pixel = bitmap->palette[pixel];
                }
                if(pixel == 0xFFFFFF){
                    pixel = bitmap->white_replacement_color;
                }
                pixels[i * width + j] = RGB888_TO_RGB565(pixel);
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

    if(o == NULL){
        return;
    }

    if(obj != NULL){
        // Find the object to remove
        while(o != NULL && o->show_obj != obj){
            o = o->next;
        }
        if(o != NULL && o->show_obj == obj){
            thingz_screen_raw_show_obj_t* next = o->next;
            thingz_screen_raw_show_obj_t* prev = o->prev;

            if(prev){
                prev->next = next;
            }else{
                raw->head = next;  // Update head to next, not NULL
            }
            if(next){
                next->prev = prev;
            }
            free(o);
        }
    }else{
        // Remove all objects
        while(o != NULL){
            thingz_screen_raw_show_obj_t* next = o->next;
            free(o);
            o = next;
        }
        raw->head = NULL;
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
    raw->bmp_block_buffer  = malloc(THINGZ_BMP_BLOCK_BUF_SIZE);
    raw->bmp_output_buffer = malloc(THINGZ_BMP_OUTPUT_BUF_SIZE * sizeof(uint16_t));
}

void thingz_screen_raw_enter(thingz_screen_raw_t *raw){
    // char* filename = thingz_get_python_file_to_exec(true);
    //thingz_print_filename(filename);
}

void thingz_screen_raw_exit(thingz_screen_raw_t *raw){
    thingz_screen_raw_show_obj_t* o = raw->head;

    while(o != NULL){
        thingz_screen_raw_show_obj_t* next = o->next;
        if (o->show_obj) {
            // Use correct type to access screen_show - offset differs between types!
            if(mp_obj_is_type(o->show_obj, &mp_thingz_display_raw_rectangle_type)){
                thingz_display_raw_rectangle_obj_t* rect = o->show_obj;
                rect->screen_show = 0;
            }else if(mp_obj_is_type(o->show_obj, &mp_thingz_display_raw_img_type)){
                thingz_display_raw_img_obj_t* img = o->show_obj;
                img->screen_show = 0;
            }else if(mp_obj_is_type(o->show_obj, &mp_thingz_display_raw_text_type)){
                thingz_display_raw_text_obj_t* text = o->show_obj;
                text->screen_show = 0;
            }
        }
        o = next;
    }
    // Note: Don't clear raw->head here - objects should persist across mode changes
}

void thingz_screen_raw_clear_objects(thingz_screen_raw_t *raw){
    // Clear all objects from the list when program terminates
    // This prevents crashes from accessing freed MicroPython objects
    raw->head = NULL;
}

static uint8_t _thingz_screen_raw_refresh_image(thingz_screen_raw_t *raw, thingz_display_raw_img_obj_t* img, uint8_t force_refresh){
    uint8_t printed = 0;
    if(img->show){
        if(img->screen_show == 0){
            // First time showing - just draw it and store bitmap metadata (width/height)
            img->bmp = thingz_screen_raw_print_bmp(&(thingz_screen.raw), img->x, img->y, img->path, img->white_replacement_color, 1);
            printed = 1;
        }else{
            // Check if image itself has changed (position or color)
            uint8_t image_changed = (img->screen_x != img->x) ||
                                   (img->screen_y != img->y) ||
                                   (img->screen_white_replacement_color != img->white_replacement_color);

            uint8_t need_refresh = force_refresh || image_changed;

            // Detect if object is COMPLETELY off-screen (not visible at all)
            uint8_t x_offscreen = (img->x >= MICROPY_THINGZ_SCREEN_WIDTH || img->x + (int16_t)img->bmp.width <= 0);
            uint8_t y_offscreen = (img->y >= MICROPY_THINGZ_SCREEN_HEIGHT || img->y + (int16_t)img->bmp.height <= 0);
            uint8_t screen_x_offscreen = (img->screen_x >= MICROPY_THINGZ_SCREEN_WIDTH || img->screen_x + (int16_t)img->bmp.width <= 0);
            uint8_t screen_y_offscreen = (img->screen_y >= MICROPY_THINGZ_SCREEN_HEIGHT || img->screen_y + (int16_t)img->bmp.height <= 0);

            // STEP 1: Draw new image first (before clearing) to reduce flicker
            // Only reload image from SPIFFS if the image itself has changed
            // Don't reload just because another object overlaps (force_refresh)
            // This avoids slow SPIFFS reads when other objects change
            if(image_changed && !x_offscreen && !y_offscreen){
                // Draw at new position (overlapping parts overwrite old pixels, reducing flicker)
                img->bmp = thingz_screen_raw_print_bmp(&(thingz_screen.raw), img->x, img->y, img->path, img->white_replacement_color, 1);
                printed = 1;
            }

            // STEP 2: Clear parts no longer covered (after drawing to minimize black flash)
            // If old or new position is completely off-screen, clear old position entirely
            if(screen_x_offscreen || screen_y_offscreen || x_offscreen || y_offscreen){
                if(!screen_x_offscreen && !screen_y_offscreen){
                    // Old position was visible, clear it (with clamping)
                    int16_t clear_x_start = (img->screen_x < 0) ? 0 : img->screen_x;
                    int16_t clear_x_end = (img->screen_x + img->bmp.width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : img->screen_x + img->bmp.width - 1;
                    int16_t clear_y_start = (img->screen_y < 0) ? 0 : img->screen_y;
                    int16_t clear_y_end = (img->screen_y + img->bmp.height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : img->screen_y + img->bmp.height - 1;
                    if(clear_x_start <= clear_x_end && clear_y_start <= clear_y_end){
                        thingz_screen_raw_fill_rect(&(thingz_screen.raw), clear_x_start, clear_x_end, clear_y_start, clear_y_end, 0);
                    }
                }
                need_refresh = 1;
            }else{
                // Both positions at least partially visible, use optimized strip clearing
                if(img->screen_x != img->x){
                    need_refresh = 1;
                    // Clear horizontal difference
                    if(img->x > img->screen_x){
                        // Moved right - clear left strip (now AFTER drawing)
                        int16_t clear_start = (img->screen_x < 0) ? 0 : img->screen_x;
                        int16_t clear_end = (img->x < 0) ? 0 : ((img->x >= MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : img->x - 1);
                        int16_t strip_y_start = (img->screen_y < 0) ? 0 : img->screen_y;
                        int16_t strip_y_end = (img->screen_y + img->bmp.height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : img->screen_y + img->bmp.height - 1;
                        if(clear_start <= clear_end && strip_y_start <= strip_y_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), clear_start, clear_end, strip_y_start, strip_y_end, 0);
                        }
                    }
                    if(img->x < img->screen_x){
                        // Moved left - clear right strip (now AFTER drawing)
                        int16_t clear_start = img->x + img->bmp.width;
                        int16_t clear_end = img->screen_x + img->bmp.width - 1;
                        // Clamp to screen bounds
                        if(clear_start < 0) clear_start = 0;
                        if(clear_end >= MICROPY_THINGZ_SCREEN_WIDTH) clear_end = MICROPY_THINGZ_SCREEN_WIDTH - 1;
                        int16_t strip_y_start = (img->screen_y < 0) ? 0 : img->screen_y;
                        int16_t strip_y_end = (img->screen_y + img->bmp.height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : img->screen_y + img->bmp.height - 1;
                        if(clear_start <= clear_end && strip_y_start <= strip_y_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), clear_start, clear_end, strip_y_start, strip_y_end, 0);
                        }
                    }
                }

                if(img->screen_y != img->y){
                    need_refresh = 1;
                    // Clear vertical difference
                    if(img->y > img->screen_y){
                        // Moved down - clear top strip (now AFTER drawing)
                        int16_t clear_start = (img->screen_y < 0) ? 0 : img->screen_y;
                        int16_t clear_end = (img->y < 0) ? 0 : ((img->y >= MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : img->y - 1);
                        int16_t strip_x_start = (img->screen_x < 0) ? 0 : img->screen_x;
                        int16_t strip_x_end = (img->screen_x + img->bmp.width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : img->screen_x + img->bmp.width - 1;
                        if(clear_start <= clear_end && strip_x_start <= strip_x_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), strip_x_start, strip_x_end, clear_start, clear_end, 0);
                        }
                    }
                    if(img->y < img->screen_y){
                        // Moved up - clear bottom strip (now AFTER drawing)
                        int16_t clear_start = img->y + img->bmp.height;
                        int16_t clear_end = img->screen_y + img->bmp.height - 1;
                        // Clamp to screen bounds
                        if(clear_start < 0) clear_start = 0;
                        if(clear_end >= MICROPY_THINGZ_SCREEN_HEIGHT) clear_end = MICROPY_THINGZ_SCREEN_HEIGHT - 1;
                        int16_t strip_x_start = (img->screen_x < 0) ? 0 : img->screen_x;
                        int16_t strip_x_end = (img->screen_x + img->bmp.width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : img->screen_x + img->bmp.width - 1;
                        if(clear_start <= clear_end && strip_x_start <= strip_x_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), strip_x_start, strip_x_end, clear_start, clear_end, 0);
                        }
                    }
                }
            }
            // TODO: If force_refresh but !image_changed, should redraw from cached bitmap
            // For now, we accept that overlapping objects won't redraw this image
        }
        img->screen_x = img->x;
        img->screen_y = img->y;
        img->screen_white_replacement_color = img->white_replacement_color;
        img->screen_show = 1;
    }else{
        if(img->screen_show){
            thingz_screen_raw_fill_rect(&(thingz_screen.raw), img->screen_x, img->screen_x+img->bmp.width-1,
                                       img->screen_y, img->screen_y+img->bmp.height-1, 0);
            img->screen_show = 0;
        }
    }
    return printed;
}

static uint8_t _thingz_screen_raw_refresh_rectangle(thingz_screen_raw_t *raw, thingz_display_raw_rectangle_obj_t* rectangle, uint8_t force_refresh){
    uint8_t printed = 0;
    if(rectangle->show){
        if(rectangle->screen_show == 0){
            // First time showing - just draw it
            thingz_screen_raw_fill_rect(&(thingz_screen.raw), rectangle->x, rectangle->x+rectangle->width-1, rectangle->y, rectangle->y+rectangle->height-1, RGB888_TO_RGB565(rectangle->color));
            printed = 1;
        }else{
            uint8_t need_refresh = force_refresh;

            // Clear parts no longer covered when position/size changes
            // Detect if rectangle is COMPLETELY off-screen
            uint8_t x_offscreen = (rectangle->x >= MICROPY_THINGZ_SCREEN_WIDTH || rectangle->x + (int16_t)rectangle->width <= 0);
            uint8_t y_offscreen = (rectangle->y >= MICROPY_THINGZ_SCREEN_HEIGHT || rectangle->y + (int16_t)rectangle->height <= 0);
            uint8_t screen_x_offscreen = (rectangle->screen_x >= MICROPY_THINGZ_SCREEN_WIDTH || rectangle->screen_x + (int16_t)rectangle->screen_width <= 0);
            uint8_t screen_y_offscreen = (rectangle->screen_y >= MICROPY_THINGZ_SCREEN_HEIGHT || rectangle->screen_y + (int16_t)rectangle->screen_height <= 0);

            // If old or new position is completely off-screen, clear old position entirely
            if(screen_x_offscreen || screen_y_offscreen || x_offscreen || y_offscreen){
                if(!screen_x_offscreen && !screen_y_offscreen){
                    int16_t clear_x_start = (rectangle->screen_x < 0) ? 0 : rectangle->screen_x;
                    int16_t clear_x_end = (rectangle->screen_x + rectangle->screen_width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : rectangle->screen_x + rectangle->screen_width - 1;
                    int16_t clear_y_start = (rectangle->screen_y < 0) ? 0 : rectangle->screen_y;
                    int16_t clear_y_end = (rectangle->screen_y + rectangle->screen_height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : rectangle->screen_y + rectangle->screen_height - 1;
                    if(clear_x_start <= clear_x_end && clear_y_start <= clear_y_end){
                        thingz_screen_raw_fill_rect(&(thingz_screen.raw), clear_x_start, clear_x_end, clear_y_start, clear_y_end, 0);
                    }
                }
                need_refresh = 1;
            }else{
                // Both positions at least partially visible, use optimized strip clearing
                if(rectangle->screen_x != rectangle->x || rectangle->screen_width != rectangle->width){
                    need_refresh = 1;
                    // Clear horizontal difference
                    if(rectangle->x > rectangle->screen_x){
                        // Moved right - clear left strip
                        int16_t clear_start = (rectangle->screen_x < 0) ? 0 : rectangle->screen_x;
                        int16_t clear_end = (rectangle->x < 0) ? 0 : ((rectangle->x >= MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : rectangle->x - 1);
                        int16_t strip_y_start = (rectangle->screen_y < 0) ? 0 : rectangle->screen_y;
                        int16_t strip_y_end = (rectangle->screen_y + rectangle->screen_height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : rectangle->screen_y + rectangle->screen_height - 1;
                        if(clear_start <= clear_end && strip_y_start <= strip_y_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), clear_start, clear_end, strip_y_start, strip_y_end, 0);
                        }
                    }
                    if(rectangle->x + rectangle->width < rectangle->screen_x + rectangle->screen_width){
                        // Shrank or moved left - clear right strip
                        int16_t clear_start = rectangle->x + rectangle->width;
                        int16_t clear_end = rectangle->screen_x + rectangle->screen_width - 1;
                        if(clear_start < 0) clear_start = 0;
                        if(clear_end >= MICROPY_THINGZ_SCREEN_WIDTH) clear_end = MICROPY_THINGZ_SCREEN_WIDTH - 1;
                        int16_t strip_y_start = (rectangle->screen_y < 0) ? 0 : rectangle->screen_y;
                        int16_t strip_y_end = (rectangle->screen_y + rectangle->screen_height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : rectangle->screen_y + rectangle->screen_height - 1;
                        if(clear_start <= clear_end && strip_y_start <= strip_y_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), clear_start, clear_end, strip_y_start, strip_y_end, 0);
                        }
                    }
                }

                if(rectangle->screen_y != rectangle->y || rectangle->screen_height != rectangle->height){
                    need_refresh = 1;
                    // Clear vertical difference
                    if(rectangle->y > rectangle->screen_y){
                        // Moved down - clear top strip
                        int16_t clear_start = (rectangle->screen_y < 0) ? 0 : rectangle->screen_y;
                        int16_t clear_end = (rectangle->y < 0) ? 0 : ((rectangle->y >= MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : rectangle->y - 1);
                        int16_t strip_x_start = (rectangle->screen_x < 0) ? 0 : rectangle->screen_x;
                        int16_t strip_x_end = (rectangle->screen_x + rectangle->screen_width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : rectangle->screen_x + rectangle->screen_width - 1;
                        if(clear_start <= clear_end && strip_x_start <= strip_x_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), strip_x_start, strip_x_end, clear_start, clear_end, 0);
                        }
                    }
                    if(rectangle->y + rectangle->height < rectangle->screen_y + rectangle->screen_height){
                        // Shrank or moved up - clear bottom strip
                        int16_t clear_start = rectangle->y + rectangle->height;
                        int16_t clear_end = rectangle->screen_y + rectangle->screen_height - 1;
                        if(clear_start < 0) clear_start = 0;
                        if(clear_end >= MICROPY_THINGZ_SCREEN_HEIGHT) clear_end = MICROPY_THINGZ_SCREEN_HEIGHT - 1;
                        int16_t strip_x_start = (rectangle->screen_x < 0) ? 0 : rectangle->screen_x;
                        int16_t strip_x_end = (rectangle->screen_x + rectangle->screen_width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : rectangle->screen_x + rectangle->screen_width - 1;
                        if(clear_start <= clear_end && strip_x_start <= strip_x_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), strip_x_start, strip_x_end, clear_start, clear_end, 0);
                        }
                    }
                }
            }

            if(rectangle->color != rectangle->screen_color){
                need_refresh = 1;
            }

            if(need_refresh && !x_offscreen && !y_offscreen){
                // Draw at new position (overlapping parts are overwritten, reducing flicker) - only if at least partially visible
                thingz_screen_raw_fill_rect(&(thingz_screen.raw), rectangle->x, rectangle->x+rectangle->width-1,
                                           rectangle->y, rectangle->y+rectangle->height-1, RGB888_TO_RGB565(rectangle->color));
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
            thingz_screen_raw_fill_rect(&(thingz_screen.raw), rectangle->screen_x, rectangle->screen_x+rectangle->screen_width-1,
                                       rectangle->screen_y, rectangle->screen_y+rectangle->screen_height-1, 0);
            rectangle->screen_show = 0;
        }
    }
    return printed;
}

static uint8_t _thingz_screen_raw_refresh_text(thingz_screen_raw_t *raw, thingz_display_raw_text_obj_t* text, uint8_t force_refresh){
    uint8_t printed = 0;
    if (!raw || !raw->screen) return 0;  // Safety check
    uint16_t len = strlen(text->text);
    uint8_t height = raw->screen->params.font_height;
    uint8_t width = raw->screen->params.font_width * len;

    if(text->show){
        if(text->screen_show == 0){
            // First time showing - just draw it
            if(len == 0)
                return printed;
            thingz_screen_raw_write(raw, text->x, text->y, text->text, len, text->color);
            printed = 1;
        }else{
            uint8_t need_refresh = force_refresh || text->has_changed || (text->color != text->screen_color);

            // Clear parts no longer covered when position/size changes
            // Detect if text is COMPLETELY off-screen
            uint8_t x_offscreen = (text->x >= MICROPY_THINGZ_SCREEN_WIDTH || text->x + (int16_t)width <= 0);
            uint8_t y_offscreen = (text->y >= MICROPY_THINGZ_SCREEN_HEIGHT || text->y + (int16_t)height <= 0);
            uint8_t screen_x_offscreen = (text->screen_x >= MICROPY_THINGZ_SCREEN_WIDTH || text->screen_x + (int16_t)text->screen_width <= 0);
            uint8_t screen_y_offscreen = (text->screen_y >= MICROPY_THINGZ_SCREEN_HEIGHT || text->screen_y + (int16_t)text->screen_height <= 0);

            // If old or new position is completely off-screen, clear old position entirely
            if(screen_x_offscreen || screen_y_offscreen || x_offscreen || y_offscreen){
                if(!screen_x_offscreen && !screen_y_offscreen){
                    int16_t clear_x_start = (text->screen_x < 0) ? 0 : text->screen_x;
                    int16_t clear_x_end = (text->screen_x + text->screen_width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : text->screen_x + text->screen_width - 1;
                    int16_t clear_y_start = (text->screen_y < 0) ? 0 : text->screen_y;
                    int16_t clear_y_end = (text->screen_y + text->screen_height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : text->screen_y + text->screen_height - 1;
                    if(clear_x_start <= clear_x_end && clear_y_start <= clear_y_end){
                        thingz_screen_raw_fill_rect(&(thingz_screen.raw), clear_x_start, clear_x_end, clear_y_start, clear_y_end, 0);
                    }
                }
                need_refresh = 1;
            }else{
                // Both positions at least partially visible, use optimized strip clearing
                if(text->screen_x != text->x || text->screen_width != width){
                    need_refresh = 1;
                    // Clear horizontal difference
                    if(text->x > text->screen_x){
                        // Moved right - clear left strip
                        int16_t clear_start = (text->screen_x < 0) ? 0 : text->screen_x;
                        int16_t clear_end = (text->x < 0) ? 0 : ((text->x >= MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : text->x - 1);
                        int16_t strip_y_start = (text->screen_y < 0) ? 0 : text->screen_y;
                        int16_t strip_y_end = (text->screen_y + text->screen_height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : text->screen_y + text->screen_height - 1;
                        if(clear_start <= clear_end && strip_y_start <= strip_y_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), clear_start, clear_end, strip_y_start, strip_y_end, 0);
                        }
                    }
                    if(text->x + width < text->screen_x + text->screen_width){
                        // Shrank or moved left - clear right strip
                        int16_t clear_start = text->x + width;
                        int16_t clear_end = text->screen_x + text->screen_width - 1;
                        if(clear_start < 0) clear_start = 0;
                        if(clear_end >= MICROPY_THINGZ_SCREEN_WIDTH) clear_end = MICROPY_THINGZ_SCREEN_WIDTH - 1;
                        int16_t strip_y_start = (text->screen_y < 0) ? 0 : text->screen_y;
                        int16_t strip_y_end = (text->screen_y + text->screen_height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : text->screen_y + text->screen_height - 1;
                        if(clear_start <= clear_end && strip_y_start <= strip_y_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), clear_start, clear_end, strip_y_start, strip_y_end, 0);
                        }
                    }
                }

                if(text->screen_y != text->y || text->screen_height != height){
                    need_refresh = 1;
                    // Clear vertical difference
                    if(text->y > text->screen_y){
                        // Moved down - clear top strip
                        int16_t clear_start = (text->screen_y < 0) ? 0 : text->screen_y;
                        int16_t clear_end = (text->y < 0) ? 0 : ((text->y >= MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT - 1 : text->y - 1);
                        int16_t strip_x_start = (text->screen_x < 0) ? 0 : text->screen_x;
                        int16_t strip_x_end = (text->screen_x + text->screen_width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : text->screen_x + text->screen_width - 1;
                        if(clear_start <= clear_end && strip_x_start <= strip_x_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), strip_x_start, strip_x_end, clear_start, clear_end, 0);
                        }
                    }
                    if(text->y + height < text->screen_y + text->screen_height){
                        // Shrank or moved up - clear bottom strip
                        int16_t clear_start = text->y + height;
                        int16_t clear_end = text->screen_y + text->screen_height - 1;
                        if(clear_start < 0) clear_start = 0;
                        if(clear_end >= MICROPY_THINGZ_SCREEN_HEIGHT) clear_end = MICROPY_THINGZ_SCREEN_HEIGHT - 1;
                        int16_t strip_x_start = (text->screen_x < 0) ? 0 : text->screen_x;
                        int16_t strip_x_end = (text->screen_x + text->screen_width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - 1 : text->screen_x + text->screen_width - 1;
                        if(clear_start <= clear_end && strip_x_start <= strip_x_end){
                            thingz_screen_raw_fill_rect(&(thingz_screen.raw), strip_x_start, strip_x_end, clear_start, clear_end, 0);
                        }
                    }
                }
            }

            if(need_refresh && !x_offscreen && !y_offscreen){
                // Draw at new position (overlapping parts are overwritten) - only if at least partially visible
                if(len > 0){
                    thingz_screen_raw_write(raw, text->x, text->y, text->text, len, text->color);
                }
                printed = 1;
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
            thingz_screen_raw_fill_rect(&(thingz_screen.raw), text->screen_x, text->screen_x+text->screen_width-1,
                                       text->screen_y, text->screen_y+text->screen_height-1, 0);
            text->screen_show = 0;
        }
    }
    return printed;
}

// Helper to extract object properties
typedef struct {
    int16_t x, y, width, height;
    int16_t sx, sy, swidth, sheight;
} obj_bounds_t;

static inline void _thingz_get_obj_bounds(mp_obj_t obj, obj_bounds_t *bounds) {
    if(mp_obj_is_type(obj, &mp_thingz_display_raw_rectangle_type)){
        thingz_display_raw_rectangle_obj_t* rect = obj;
        bounds->x = rect->x;
        bounds->y = rect->y;
        bounds->width = rect->width;
        bounds->height = rect->height;
        bounds->sx = rect->screen_x;
        bounds->sy = rect->screen_y;
        bounds->swidth = rect->screen_width;
        bounds->sheight = rect->screen_height;
    }else if(mp_obj_is_type(obj, &mp_thingz_display_raw_img_type)){
        thingz_display_raw_img_obj_t* img = obj;
        bounds->x = img->x;
        bounds->y = img->y;
        bounds->width = img->bmp.width;
        bounds->height = img->bmp.height;
        bounds->sx = img->screen_x;
        bounds->sy = img->screen_y;
        bounds->swidth = img->bmp.width;
        bounds->sheight = img->bmp.height;
    }else if(mp_obj_is_type(obj, &mp_thingz_display_raw_text_type)){
        thingz_display_raw_text_obj_t* text = obj;
        bounds->x = text->x;
        bounds->y = text->y;
        bounds->width = text->screen_width;
        bounds->height = text->screen_height;
        bounds->sx = text->screen_x;
        bounds->sy = text->screen_y;
        bounds->swidth = text->screen_width;
        bounds->sheight = text->screen_height;
    }
}

static uint8_t _thingz_screen_raw_is_overlapping(thingz_screen_raw_show_obj_t* o1, thingz_screen_raw_show_obj_t* o2){
    obj_bounds_t b1 = {0}, b2 = {0};
    _thingz_get_obj_bounds(o1->show_obj, &b1);
    _thingz_get_obj_bounds(o2->show_obj, &b2);

    // Proper 2D rectangle overlap: both X and Y ranges must overlap.
    // The previous OR-only check failed when one rect fully contains the other
    // (e.g. a full-screen image covering a small paddle).
    uint8_t x_overlap = (b1.sx < b2.sx + b2.swidth) && (b1.sx + b1.swidth > b2.sx);
    uint8_t y_overlap = (b1.sy < b2.sy + b2.sheight) && (b1.sy + b1.sheight > b2.sy);

    return (x_overlap && y_overlap)
    &&(    //if there is no change of object o2 we don't need to refresh o1
           (b2.sx != b2.x)
    ||     (b2.sy != b2.y)
    ||     (b2.swidth != b2.width)
    ||     (b2.sheight != b2.height)
    ||     (o2->was_updated)
    );
}

static uint8_t _thingz_screen_raw_is_obj_on_top(thingz_screen_raw_show_obj_t* current){
    // Check if any lower-z object (prev chain) changed and overlaps current.
    // current must then redraw to stay on top of the changed lower-z object.
    thingz_screen_raw_show_obj_t* obj = current->prev;
    while(obj != NULL){
        if(_thingz_screen_raw_is_overlapping(current, obj)){
            return 1;
        }
        obj = obj->prev;
    }
    return 0;
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

    thingz_screen_raw_t* raw = r;
    // Check for NULL pointers before dereferencing
    if (!raw || !raw->screen) {
        refresh_in_progress = false;
        return mp_const_none;
    }
    if (gc_is_locked()) {
        raw->head = NULL;
        refresh_in_progress = false;
        return mp_const_none;
    }
    if (raw->screen->current_mode != COMMON_THINGZ_SCREEN_MODE_RAW) {
        // Mode changed - clear flag to allow future refreshes
        refresh_in_progress = false;
        return mp_const_none;
    }
    // Try to take semaphore with SHORT timeout (2ms) to avoid long blocking in Python context
    // Balance between preventing corruption and reducing stuttering
    if(xSemaphoreTake(controlLcd, pdMS_TO_TICKS(2)) == pdFALSE){
        // Clear flag to allow retry on next timer tick
        refresh_in_progress = false;
        return mp_const_none;
    }
    // Pre-pass: reset flags and clear old areas BEFORE any drawing.
    // For images: clear only the vacated strips (not the full area) to avoid flicker
    //   and unnecessary SPI writes. screen_show=0 causes the main pass to use the
    //   "first-time show" path which redraws at the new position without strip clearing.
    // For rectangles/texts: clear the full old area (fill_rect is cheap, no flash read).
    thingz_screen_raw_show_obj_t* obj = raw->head;
    while(obj != NULL) {
        obj->was_updated = 0;
        if (obj->show_obj) {
            if(mp_obj_is_type(obj->show_obj, &mp_thingz_display_raw_img_type)){
                thingz_display_raw_img_obj_t* img = obj->show_obj;
                if (img->screen_show) {
                    if (!img->show) {
                        // Becoming invisible: clear full area
                        thingz_screen_raw_fill_rect(&(thingz_screen.raw), img->screen_x, img->screen_x+img->bmp.width-1,
                                                   img->screen_y, img->screen_y+img->bmp.height-1, 0);
                        img->screen_show = 0;
                        obj->was_updated = 1;
                    } else {
                        int16_t dx = img->x - img->screen_x;
                        int16_t dy = img->y - img->screen_y;
                        uint8_t color_changed = (img->screen_white_replacement_color != img->white_replacement_color);
                        if (dx != 0 || dy != 0 || color_changed) {
                            if (color_changed && dx == 0 && dy == 0) {
                                // Color only: clear full area
                                thingz_screen_raw_fill_rect(&(thingz_screen.raw), img->screen_x, img->screen_x+img->bmp.width-1,
                                                           img->screen_y, img->screen_y+img->bmp.height-1, 0);
                            } else {
                                // Position changed: clear only the vacated strips
                                if (dx > 0) {
                                    thingz_screen_raw_fill_rect(&(thingz_screen.raw),
                                        img->screen_x, img->x - 1,
                                        img->screen_y, img->screen_y + img->bmp.height - 1, 0);
                                } else if (dx < 0) {
                                    thingz_screen_raw_fill_rect(&(thingz_screen.raw),
                                        img->x + img->bmp.width, img->screen_x + img->bmp.width - 1,
                                        img->screen_y, img->screen_y + img->bmp.height - 1, 0);
                                }
                                if (dy > 0) {
                                    thingz_screen_raw_fill_rect(&(thingz_screen.raw),
                                        img->screen_x, img->screen_x + img->bmp.width - 1,
                                        img->screen_y, img->y - 1, 0);
                                } else if (dy < 0) {
                                    thingz_screen_raw_fill_rect(&(thingz_screen.raw),
                                        img->screen_x, img->screen_x + img->bmp.width - 1,
                                        img->y + img->bmp.height, img->screen_y + img->bmp.height - 1, 0);
                                }
                            }
                            img->screen_show = 0;
                            obj->was_updated = 1;
                        }
                    }
                }
            } else if(mp_obj_is_type(obj->show_obj, &mp_thingz_display_raw_rectangle_type)){
                thingz_display_raw_rectangle_obj_t* rect = obj->show_obj;
                if (rect->screen_show) {
                    uint8_t changed = !rect->show
                        || (rect->screen_x != rect->x) || (rect->screen_y != rect->y)
                        || (rect->screen_width != rect->width) || (rect->screen_height != rect->height)
                        || (rect->screen_color != rect->color);
                    if (changed) {
                        thingz_screen_raw_fill_rect(&(thingz_screen.raw), rect->screen_x, rect->screen_x+rect->screen_width-1,
                                                   rect->screen_y, rect->screen_y+rect->screen_height-1, 0);
                        rect->screen_show = 0;
                        obj->was_updated = 1;
                    }
                }
            } else if(mp_obj_is_type(obj->show_obj, &mp_thingz_display_raw_text_type)){
                thingz_display_raw_text_obj_t* text = obj->show_obj;
                if (text->screen_show) {
                    uint8_t changed = !text->show || text->has_changed
                        || (text->screen_x != text->x) || (text->screen_y != text->y)
                        || (text->screen_color != text->color);
                    if (changed) {
                        thingz_screen_raw_fill_rect(&(thingz_screen.raw), text->screen_x, text->screen_x+text->screen_width-1,
                                                   text->screen_y, text->screen_y+text->screen_height-1, 0);
                        text->screen_show = 0;
                        obj->was_updated = 1;
                    }
                }
            }
        }
        obj = obj->next;
    }

    // Main pass: refresh all objects in z-order (head=low-z, tail=high-z).
    // was_updated from the pre-pass drives force_refresh for objects uncovered by the cleared areas.
    obj = raw->head;
    while(obj != NULL){
        if (!obj->show_obj) {
            obj = obj->next;
            continue;
        }

        uint8_t force_refresh = _thingz_screen_raw_is_obj_on_top(obj);
        if(!force_refresh){
            force_refresh = _thingz_screen_raw_is_obj_under(obj);
        }
        if(mp_obj_is_type(obj->show_obj, &mp_thingz_display_raw_rectangle_type)){
            obj->was_updated |= _thingz_screen_raw_refresh_rectangle(raw, obj->show_obj, force_refresh);
        }else if(mp_obj_is_type(obj->show_obj, &mp_thingz_display_raw_img_type)){
            obj->was_updated |= _thingz_screen_raw_refresh_image(raw, obj->show_obj, force_refresh);
        }else if(mp_obj_is_type(obj->show_obj, &mp_thingz_display_raw_text_type)){
            obj->was_updated |= _thingz_screen_raw_refresh_text(raw, obj->show_obj, force_refresh);
        }
        obj = obj->next;
    }
    xSemaphoreGive(controlLcd);  // Release semaphore
    refresh_in_progress = false;  // Clear flag after work is done

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_screen_raw_refresh_obj, mp_thingz_screen_raw_refresh);


bool thingz_screen_raw_refresh(thingz_screen_raw_t *raw){
    // Called from the dedicated Python refresh task in thingz_screen.c
    // which already has the GIL acquired
    mp_call_function_1((mp_obj_t)&mp_thingz_screen_raw_refresh_obj, MP_OBJ_FROM_PTR(raw));
    return true;
}

mp_uint_t thingz_screen_raw_write(thingz_screen_raw_t *raw, int16_t x, int16_t y, const void *buf, mp_uint_t size, uint32_t color){
    // Clamp coordinates for partial visibility
    if (!raw || !raw->screen) return size;  // Safety check
    if(x < 0 || x >= MICROPY_THINGZ_SCREEN_WIDTH || y < 0 || y >= MICROPY_THINGZ_SCREEN_HEIGHT) return size;
    thingz_screen_print_screen(buf, size, x, MICROPY_THINGZ_SCREEN_HEIGHT-1-y-raw->screen->params.font_height+1, RGB888_TO_RGB565(color), 1);
    return size;
}

thingz_screen_bitmap_t thingz_screen_raw_print_bmp(thingz_screen_raw_t *raw, int16_t x, int16_t y, const char *file, uint32_t white_replacement_color, uint8_t show){
    ESP_LOGW("BMP", "A: start load %s", file);
    thingz_screen_bitmap_t bitmap;
    bitmap.shown = 0;
    mp_obj_t args[] = {
        mp_obj_new_str(file, strlen(file)),
        mp_obj_new_str("rb", 2),
    };
    uint16_t bmp_header[69];
    ESP_LOGW("BMP", "B: before vfs_open");
    pyb_file_obj_t* f = mp_vfs_open(2, args, &mp_const_empty_map);
    ESP_LOGW("BMP", "C: after vfs_open");
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
        // Calculate visible portion of image for clipping
        int16_t clip_x_start = (x < 0) ? 0 : x;
        int16_t clip_y_start = (y < 0) ? 0 : y;
        int16_t clip_x_end = (x + bitmap.width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH : x + bitmap.width;
        int16_t clip_y_end = (y + bitmap.height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT : y + bitmap.height;

        // Check if image is completely off-screen
        if(x >= MICROPY_THINGZ_SCREEN_WIDTH || y >= MICROPY_THINGZ_SCREEN_HEIGHT ||
           x + bitmap.width <= 0 || y + bitmap.height <= 0) {
            // Completely off-screen, skip drawing but keep bitmap metadata
            if(bitmap.palette) m_free(bitmap.palette);
            f_close(&f->fp);
            return bitmap;
        }

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

        // Calculate which rows to draw (for clipping in X)
        int row_start = (x < 0) ? -x : 0;
        int row_end = (x + bitmap.width > MICROPY_THINGZ_SCREEN_WIDTH) ? MICROPY_THINGZ_SCREEN_WIDTH - x : bitmap.width;

        // Calculate visible height for clipping in Y (with rotation, Y maps to LCD X)
        int16_t visible_y_start = (y < 0) ? 0 : y;
        int16_t visible_y_end = (y + bitmap.height > MICROPY_THINGZ_SCREEN_HEIGHT) ? MICROPY_THINGZ_SCREEN_HEIGHT : y + bitmap.height;
        int16_t visible_height = visible_y_end - visible_y_start;
        int16_t y_offset = (y < 0) ? -y : 0;  // Offset into bitmap if partially off top

        if(visible_height <= 0) {
            // No visible portion in Y
            return bitmap;
        }

        // Guard: pre-allocated block buffer covers images up to screen_width wide.
        // Skip rendering if the image stride exceeds the buffer (image too wide for screen).
        if (bitmap.stride > MICROPY_THINGZ_SCREEN_WIDTH * 4 || !raw->bmp_block_buffer || !raw->bmp_output_buffer) {
            f_close(&f->fp);
            if (bitmap.palette) m_free(bitmap.palette);
            return bitmap;
        }

        uint8_t  *block_buffer  = raw->bmp_block_buffer;
        uint16_t *output_buffer = raw->bmp_output_buffer;


        // Process columns in groups
        int col = row_start;
        while(col < row_end) {
            // Determine how many columns in this group
            int cols_in_group = (col + THINGZ_BMP_GROUP_COLS <= row_end) ? THINGZ_BMP_GROUP_COLS : (row_end - col);

            // Calculate screen coordinates for this group
            int16_t draw_y_start = x + col;
            int16_t draw_y_end = x + col + cols_in_group - 1;

            // Check if any column in this group is visible on screen
            if(draw_y_end < 0 || draw_y_start >= MICROPY_THINGZ_SCREEN_WIDTH) {
                col += cols_in_group;
                continue;  // Skip this group entirely
            }

            // Clamp to visible screen area
            int first_visible_col = 0;
            int last_visible_col = cols_in_group - 1;
            if(draw_y_start < 0) {
                first_visible_col = -draw_y_start;
                draw_y_start = 0;
            }
            if(draw_y_end >= MICROPY_THINGZ_SCREEN_WIDTH) {
                last_visible_col = cols_in_group - 1 - (draw_y_end - MICROPY_THINGZ_SCREEN_WIDTH + 1);
                draw_y_end = MICROPY_THINGZ_SCREEN_WIDTH - 1;
            }
            int visible_cols_in_group = last_visible_col - first_visible_col + 1;

            // Clear output buffer
            memset(output_buffer, 0, sizeof(uint16_t) * cols_in_group * visible_height);

            // Read BMP rows in blocks and extract columns
            // BMP is stored bottom-to-top, visible area is from y_offset to y_offset+visible_height-1
            int bmp_row = y_offset;
            int out_row = visible_height - 1;  // Fill output from bottom (for rotation)

            while(bmp_row < y_offset + visible_height) {
                // Calculate how many lines to read in this block
                int lines_remaining = (y_offset + visible_height) - bmp_row;
                int lines_to_read = (lines_remaining > THINGZ_BMP_BLOCK_LINES) ? THINGZ_BMP_BLOCK_LINES : lines_remaining;

                // Seek to the start of this block in the file
                // BMP row N is at file offset: data_offset + (height-1-N) * stride
                uint32_t first_bmp_row_in_block = bmp_row + lines_to_read - 1;
                uint32_t file_row = bitmap.height - 1 - first_bmp_row_in_block;
                uint32_t file_offset = bitmap.data_offset + file_row * bitmap.stride;
                f_lseek(&f->fp, file_offset);

                // Read multiple lines at once (sequential read, much faster than individual seeks)
                UINT bytes_read_block;
                f_read(&f->fp, block_buffer, lines_to_read * bitmap.stride, &bytes_read_block);

                // Process each line in the block
                for(int block_line = 0; block_line < lines_to_read && out_row >= 0; block_line++) {
                    // Line in block_buffer (block_line=0 is the highest BMP row we read)
                    uint8_t *line_data = block_buffer + (lines_to_read - 1 - block_line) * bitmap.stride;

                    // Extract pixels for columns in current group
                    for(int c = 0; c < cols_in_group; c++) {
                        int bmp_col = col + c;
                        uint32_t pixel_data = 0;

                        // Extract pixel from line_data based on format
                        if(pixels_per_byte > 0) {
                            // Sub-byte pixels (1, 4 bpp)
                            int byte_idx = bmp_col / pixels_per_byte;
                            pixel_data = line_data[byte_idx];
                            int offset = (bmp_col % pixels_per_byte) * bitmap.bits_per_pixel;
                            uint8_t mask = (1 << bitmap.bits_per_pixel) - 1;
                            pixel_data = (pixel_data >> ((8 - bitmap.bits_per_pixel) - offset)) & mask;
                        } else {
                            // Multi-byte pixels (8, 16, 24, 32 bpp)
                            int byte_idx = bmp_col * bytes_per_pixel;
                            for(int b = 0; b < bytes_per_pixel; b++) {
                                pixel_data |= ((uint32_t)line_data[byte_idx + b]) << (b * 8);
                            }
                            pixel_data = _thingz_decode_pixel(pixel_data, bytes_per_pixel, pixels_per_byte, bitmap.bits_per_pixel, bmp_col, &bitmap);
                        }

                        // Apply palette if indexed
                        if(bitmap.palette != NULL) {
                            pixel_data = bitmap.palette[pixel_data];
                        }

                        // Replace white
                        if(pixel_data == 0xFFFFFF) {
                            pixel_data = bitmap.white_replacement_color;
                        }

                        // Convert to RGB565 and store in output buffer
                        // Output is organized as: [col0_row0, col0_row1, ..., col0_rowN, col1_row0, ...]
                        output_buffer[c * visible_height + out_row] = RGB888_TO_RGB565(pixel_data);
                    }
                    out_row--;
                }

                bmp_row += lines_to_read;
            }

            // Send visible columns via SPI
            // Wait for previous async transaction
            if(thingz_screen.pendingTrans) {
                spi_wait_for_pending_trans(&(thingz_screen.dev), (spi_transaction_t*)thingz_screen.pendingTrans);
                thingz_screen.pendingTrans = NULL;
            }

            // Select buffer and transaction structure
            uint8_t* byte_buffer = (uint8_t*)(thingz_screen.activeBuffer == 0 ? thingz_screen.lineData : thingz_screen.lineData2);
            spi_transaction_t* trans_struct = (spi_transaction_t*)thingz_screen.transPool[thingz_screen.activeBuffer];

            // Convert only visible columns to big-endian bytes for SPI
            int byte_index = 0;
            for(int c = first_visible_col; c <= last_visible_col; c++) {
                for(int r = 0; r < visible_height; r++) {
                    uint16_t color = output_buffer[c * visible_height + r];
                    byte_buffer[byte_index++] = (color >> 8) & 0xFF;
                    byte_buffer[byte_index++] = color & 0xFF;
                }
            }

            // Setup LCD window for grouped columns
            int16_t draw_x_coord = MICROPY_THINGZ_SCREEN_HEIGHT - visible_height - visible_y_start;

            uint16_t lcd_x1 = draw_x_coord + thingz_screen.dev._offsetx;
            uint16_t lcd_x2 = lcd_x1 + visible_height - 1;
            uint16_t lcd_y1 = draw_y_start + thingz_screen.dev._offsety;
            uint16_t lcd_y2 = draw_y_end + thingz_screen.dev._offsety;

            if (thingz_screen.dev._model == 0x7735) {
                spi_master_write_comm_byte(&(thingz_screen.dev), 0x2A);
                spi_master_write_data_word(&(thingz_screen.dev), lcd_x1);
                spi_master_write_data_word(&(thingz_screen.dev), lcd_x2);
                spi_master_write_comm_byte(&(thingz_screen.dev), 0x2B);
                spi_master_write_data_word(&(thingz_screen.dev), lcd_y1);
                spi_master_write_data_word(&(thingz_screen.dev), lcd_y2);
                spi_master_write_comm_byte(&(thingz_screen.dev), 0x2C);

                // Send async with DMA
                spi_master_write_colors_async(&(thingz_screen.dev), byte_buffer, visible_cols_in_group * visible_height * 2, trans_struct);
                thingz_screen.pendingTrans = trans_struct;

                // Toggle buffer
                thingz_screen.activeBuffer = 1 - thingz_screen.activeBuffer;
            }

            bitmap.shown = 1;
            col += cols_in_group;
        }

        // Wait for final async transaction
        if(thingz_screen.pendingTrans) {
            spi_wait_for_pending_trans(&(thingz_screen.dev), (spi_transaction_t*)thingz_screen.pendingTrans);
            thingz_screen.pendingTrans = NULL;
        }

        ESP_LOGW("BMP", "G: render complete");

    }

    ESP_LOGW("BMP", "H: before close file");
    f_close(&f->fp);
    if(bitmap.palette)
        m_free(bitmap.palette);
    ESP_LOGW("BMP", "I: done load");

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

void thingz_screen_raw_fill_rect(thingz_screen_raw_t* raw, int16_t x, int16_t x2, int16_t y, int16_t y2, uint16_t color){
    // Clamp coordinates to screen bounds (handle partial visibility)
    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x2 >= MICROPY_THINGZ_SCREEN_WIDTH) x2 = MICROPY_THINGZ_SCREEN_WIDTH - 1;
    if(y2 >= MICROPY_THINGZ_SCREEN_HEIGHT) y2 = MICROPY_THINGZ_SCREEN_HEIGHT - 1;

    // Ensure proper ordering and valid rectangle
    if(x > x2 || y > y2 || x >= MICROPY_THINGZ_SCREEN_WIDTH || y >= MICROPY_THINGZ_SCREEN_HEIGHT) return;

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
