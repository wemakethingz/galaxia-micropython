// ST7735 screen model of the WebAssembly build (see st7735.c)
#ifndef GALAXIA_ST7735_H
#define GALAXIA_ST7735_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ST7735_PICTURE_W 160
#define ST7735_PICTURE_H 128

void st7735_reset(void);
// Bytes clocked in on the SPI bus, dc = level of the D/C line (0: command)
void st7735_write(bool dc, const uint8_t *bytes, size_t n);
// Counter of the changes of what the panel shows
uint32_t st7735_frame(void);
// The picture as seen on the board, ST7735_PICTURE_W x _H RGBA
const uint8_t *st7735_picture(void);

#endif
