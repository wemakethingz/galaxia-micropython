#ifndef GT_EESTI_H_
#define GT_EESTI_H_

#include "stdint.h"

#define GT_EESTI_OFFSET 0x20

typedef struct font_glyph_dsc{
    uint8_t w_px;
    uint16_t glyph_index;
} font_glyph_dsc_t;

const uint8_t* gt_eesti_get_bitmap();
font_glyph_dsc_t gt_eesti_get_glyph_desc(uint8_t ascii);
#endif