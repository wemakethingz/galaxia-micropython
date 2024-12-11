// Header File for SSD1306 characters
// Generated with TTF2BMH
// Font Inconsolata Light
// Font Size: 16

#include <stdint.h>

typedef struct glyp{
	const uint8_t* bitmap;
	uint16_t index;
	uint8_t width;
    uint8_t real_width;
    uint8_t height;
} glyph_t;

const uint8_t* font_get_glyph(uint16_t ascii);
const uint8_t* font_get_glyph_from_index(uint8_t index);
uint8_t font_get_glyph_index(uint16_t codepoint);