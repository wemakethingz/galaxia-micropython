#ifndef COMMON_THINGZ_SCREEN_H_
#define COMMON_THINGZ_SCREEN_H_

#include "py/obj.h"
#include "py/stream.h"

#include "lib/tft/ili9340.h"

#include "thingz_screen_repl.h"
#include "thingz_screen_plot.h"
#include "thingz_screen_debug.h"
#include "thingz_screen_raw.h"

typedef struct {
	uint8_t offsetx;
	uint8_t offsety;
	uint8_t font_height;
	uint8_t font_width;
} thingz_screen_params_t;

typedef struct thingz_screen_obj{
	mp_obj_base_t base;
	TFT_t dev;
	uint8_t current_mode;
	thingz_screen_params_t params;
	thingz_screen_repl_t repl;
	thingz_screen_plot_t plot;
	thingz_screen_debug_t debug;
	thingz_screen_raw_t raw;
	uint8_t autorefresh;

	uint16_t *lineData;
    uint16_t lineDataSize;

}  thingz_screen_obj_t;

extern const mp_obj_type_t thingz_screen_type;
extern thingz_screen_obj_t thingz_screen;

#define COMMON_THINGZ_SCREEN_MODE_REPL 0
#define COMMON_THINGZ_SCREEN_MODE_PLOT 1
#define COMMON_THINGZ_SCREEN_MODE_DEBUG 2
#define COMMON_THINGZ_SCREEN_MODE_SPLASH 3
#define COMMON_THINGZ_SCREEN_MODE_RAW 4

void thingz_screen_init(void);
// void thingz_deinit(void);

void thingz_screen_switch_mode(uint8_t mode);
uint8_t thingz_screen_get_mode();

void thingz_screen_print_screen(uint8_t* str, uint32_t len, uint32_t x, uint32_t y, uint16_t foreground, uint8_t utf8);
void thingz_screen_print_screen_with_glyp_index(uint8_t* str, uint32_t len, uint32_t x, uint32_t y, uint16_t foreground);

void thingz_screen_print_header(const char* filename);
void thingz_screen_clear();

void thingz_screen_show_splash();

void thingz_screen_autorefresh(uint8_t autorefresh);

#endif