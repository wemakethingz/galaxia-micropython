#include "font.h"

#include "py/unicode.h"
#include "py/runtime.h"


const uint32_t font_bitmap_data[294] = {
0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x40000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00400000, 0x00000000, 0x00000000, 
0x00050000, 0x00080000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000200, 0x20000000, 0x00000080, 0x80000000, 0x00000000, 0x00000000, 0x00000480, 0x00800000, 0x00000000, 0x08000000, 
0x00851421, 0x22081100, 0x00000002, 0x70871c0b, 0xe73e71c0, 0x0000001c, 0x71cf1ce3, 0xef9c89c3, 0xa282289c, 0xf1cf1cfa, 0x28a28a2f, 0x9c41c500, 0x00080008, 0x01808080, 0x90600000, 0x00000020, 0x00000000, 0x06218a80, 0x03e40040, 0x42082080, 0x08000000, 
0x00851472, 0xa5082080, 0x00000002, 0x8988a21a, 0x08028a20, 0x00080422, 0x8a28a292, 0x08228881, 0x248368a2, 0x8a28a222, 0x28a28a20, 0x90404880, 0x00080008, 0x02008000, 0x10200000, 0x00000020, 0x00000000, 0x08204905, 0xa2020020, 0x85145140, 0x10000000, 
0x00803ea9, 0x45004045, 0x08000004, 0x9888822a, 0x08028a22, 0x0813e222, 0x9a28a08a, 0x08208881, 0x2882aca2, 0x8a28a022, 0x28a25141, 0x10204000, 0x01cf1c79, 0xc71ef181, 0x9223cf1c, 0xf1eb9e72, 0x28a28a2f, 0x8820400a, 0x52071c71, 0xc7187227, 0x00000000, 
0x008014a0, 0x42004042, 0x08000004, 0xa8808c4b, 0xcf047222, 0x08200104, 0xaa2f208b, 0xcf20f881, 0x3082aaa2, 0x8a289c22, 0x25222142, 0x10204000, 0x0028a28a, 0x22228880, 0x9422a8a2, 0x8a2c2022, 0x28a25221, 0x10202014, 0x2bc0a28a, 0x28888a2a, 0x80000000, 
0x00801470, 0x8680404f, 0xbe03e008, 0xc8810288, 0x288489e0, 0x00400088, 0xabe8a08a, 0x082e8881, 0x308229a2, 0xf22f0222, 0x252a2084, 0x10104000, 0x01e8a08b, 0xe2228880, 0x9822a8a2, 0x8a281c22, 0x252a2222, 0x08204028, 0x1607a0fb, 0xef888a2a, 0x80000000, 
0x00003e28, 0xa9004042, 0x08000008, 0x888202f8, 0x28888820, 0x0023e100, 0x9a28a08a, 0x08228889, 0x288228a2, 0x822a0222, 0x252a5088, 0x10104000, 0x0228a08a, 0x02228880, 0x9822a8a2, 0x8a280222, 0x252a2224, 0x08204014, 0x2a08a082, 0x08088a2b, 0x00000000, 
0x008014a9, 0x59002085, 0x08200210, 0x8884220a, 0x28888822, 0x08100208, 0x8228a292, 0x08228889, 0x248228a2, 0x82a92222, 0x22368888, 0x10084000, 0x0228a28a, 0x02228880, 0x9422a8a2, 0x8a280222, 0x222a5228, 0x0820400a, 0x5208a282, 0x08088a2a, 0x00000000, 
0x00801471, 0x26801100, 0x00200210, 0x71cf9c09, 0xc70871c2, 0x08080408, 0x7a2f1ce3, 0xe81c89c6, 0x22fa289c, 0x81c89c21, 0xc222888f, 0x9c09c000, 0x01ef1c79, 0xe21e89c0, 0x9272a89c, 0xf1e83c19, 0xe21c89ef, 0x86218005, 0xa3e79c79, 0xe79c71e7, 0x80000000, 
0x00000020, 0x00000000, 0x00400000, 0x00000000, 0x00000000, 0x10000000, 0x00000000, 0x00000000, 0x00000000, 0x00200000, 0x00000000, 0x0000003e, 0x00000000, 0x00020004, 0x80000000, 0x80200000, 0x00000020, 0x00000000, 0x00000800, 0x00000000, 0x00000000, 
0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x001c0003, 0x00000000, 0x80200000, 0x000001c0, 0x00000000, 0x00001000, 0x00000000, 0x00000000, 
};

    // .width = 648,
    // .height = 14,
    // .data = (uint32_t*) font_bitmap_data,
    // .stride = 21.0,
    // .bits_per_value = 1,
    // .x_shift = 5,
    // .x_mask = 0x1f,
    // .bitmask = 0x1,

static uint8_t glyph[84];

static const uint8_t unicode_characters[] = "«»Éàçèéêîôûœ’";
static const uint16_t unicode_characters_len = 27;

uint32_t font_get_pixel(int16_t x, int16_t y) {
    uint16_t width = 648;
    uint16_t height = 14;
    uint16_t stride = 21;
    uint8_t bits_per_value = 1;
    uint8_t x_shift = 5;
    size_t x_mask = 0x1f;
    uint16_t bitmask = 1;

    int32_t row_start = y * stride;
    uint32_t bytes_per_value = bits_per_value / 8;
    if (bytes_per_value < 1) {
        uint32_t word = font_bitmap_data[row_start + (x >> x_shift)];
        
        return (word >> (sizeof(uint32_t) * 8 - ((x & x_mask) + 1) * bits_per_value)) & bitmask;
    } else {
        uint32_t *row = font_bitmap_data + row_start;
        if (bytes_per_value == 1) {
            return ((uint8_t *)row)[x];
        } else if (bytes_per_value == 2) {
            return ((uint16_t *)row)[x];
        } else if (bytes_per_value == 4) {
            return ((uint32_t *)row)[x];
        }
    }
    return 0;
}

uint8_t font_get_glyph_index(uint16_t codepoint) {
    if (codepoint >= 0x20 && codepoint <= 0x7e) {
        return codepoint - 0x20;
    }
    // Do a linear search of the mapping for unicode.
    const byte *j = unicode_characters;
    uint8_t k = 0;
    while (j < unicode_characters + unicode_characters_len) {
        unichar potential_c = utf8_get_char(j);
        j = utf8_next_char(j);
        if (codepoint == potential_c) {
            return 0x7f - 0x20 + k;
        }
        k++;
    }
    return 0xff;
}

const uint8_t* font_get_glyph(uint16_t ascii){
    int i,j;
    uint16_t index = font_get_glyph_index(ascii);
    
    for(i = 0; i < 6; i++){
        for(j=0; j<14; j++){
            if(j == 0){
                glyph[0] =0;
            }else{
                glyph[i*14+j] = index == 0xff ? 0: font_get_pixel(index*6+i, 14-j);
            }
        }
    }
    return glyph;
}

const uint8_t* font_get_glyph_from_index(uint8_t index){
    int i,j;

    for(i = 0; i < 6; i++){
        for(j=0; j<14; j++){
            if(j == 0){
                glyph[0] =0;
            }else{
                glyph[i*14+j] = index == 0xff ? 0: font_get_pixel(index*6+i, 14-j);
            }
        }
    }
    return glyph;
}