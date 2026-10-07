// Sitronix ST7735 controller fed by the SPI shim, for the WebAssembly build: the
// logic of galaxia_sim's QEMU model (hw/display/st7735.c in patches/), without QEMU.
//
// 132 x 162 RGB565 GRAM; the panel shows its 128 x 160 window at (2, 1), mounted
// rotated by 270 degrees: the picture is 160 x 128, as seen on the board.
// Commands: SWRESET, SLPIN/SLPOUT, INVOFF/INVON, DISPOFF/DISPON, CASET, RASET,
// RAMWR (MADCTL MY/MX/MV mapping, wrap-around), MADCTL (with BGR), COLMOD.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "st7735.h"

#define GRAM_W 132
#define GRAM_H 162
#define PANEL_W 128
#define PANEL_H 160
#define X_OFFSET 2
#define Y_OFFSET 1

enum {
    CMD_NOP = 0x00, CMD_SWRESET = 0x01, CMD_SLPIN = 0x10, CMD_SLPOUT = 0x11,
    CMD_INVOFF = 0x20, CMD_INVON = 0x21, CMD_DISPOFF = 0x28, CMD_DISPON = 0x29,
    CMD_CASET = 0x2A, CMD_RASET = 0x2B, CMD_RAMWR = 0x2C, CMD_MADCTL = 0x36, CMD_COLMOD = 0x3A,
};

#define MADCTL_MY  (1 << 7)
#define MADCTL_MX  (1 << 6)
#define MADCTL_MV  (1 << 5)
#define MADCTL_BGR (1 << 3)

static struct {
    uint16_t gram[GRAM_H][GRAM_W];
    uint8_t cmd;
    uint8_t params[4];
    int nparams;
    uint8_t pixel_bytes[3];
    int npixel_bytes;
    bool sleeping, display_on, inverted;
    uint8_t madctl, colmod;
    uint16_t xs, xe, ys, ye, x, y;
    uint32_t frame;         // changes of what the panel shows
} s;

// The picture: SCREEN_W x SCREEN_H RGBA
static uint8_t picture[ST7735_PICTURE_H][ST7735_PICTURE_W][4];

static void changed(void) {
    s.frame++;
}

void st7735_reset(void) {
    s.cmd = CMD_NOP;
    s.nparams = 0;
    s.npixel_bytes = 0;
    s.sleeping = true;
    s.display_on = false;
    s.inverted = false;
    s.madctl = 0;
    s.colmod = 0x06;
    s.xs = 0;
    s.xe = GRAM_W - 1;
    s.ys = 0;
    s.ye = GRAM_H - 1;
    s.x = s.y = 0;
    changed();
}

static void put_pixel(uint16_t rgb565) {
    unsigned col = s.x, row = s.y;
    if (s.madctl & MADCTL_MV) {
        unsigned t = col;
        col = row;
        row = t;
    }
    if (s.madctl & MADCTL_MX) {
        col = GRAM_W - 1 - col;
    }
    if (s.madctl & MADCTL_MY) {
        row = GRAM_H - 1 - row;
    }
    if (col < GRAM_W && row < GRAM_H && s.gram[row][col] != rgb565) {
        s.gram[row][col] = rgb565;
        changed();
    }
    // The address counters count up (16 bits, wrapping) until the end address, even from
    // a start beyond it: the firmware's splash text starts at a "negative" row when it is
    // wider than the screen (RASET 0xFFEC..159), the rows out of the GRAM are dropped
    // and the rest of the text is drawn, as on the board.
    if (s.x != s.xe) {
        s.x++;
    } else {
        s.x = s.xs;
        s.y = s.y != s.ye ? s.y + 1 : s.ys;
    }
}

static uint16_t rgb444_to_565(unsigned r, unsigned g, unsigned b) {
    return (((r << 1) | (r >> 3)) << 11) | (((g << 2) | (g >> 2)) << 5) | ((b << 1) | (b >> 3));
}

static void pixel_byte(uint8_t b) {
    s.pixel_bytes[s.npixel_bytes++] = b;
    const uint8_t *p = s.pixel_bytes;
    switch (s.colmod & 0x7) {
        case 0x3:
            if (s.npixel_bytes == 3) {
                put_pixel(rgb444_to_565(p[0] >> 4, p[0] & 0xf, p[1] >> 4));
                put_pixel(rgb444_to_565(p[1] & 0xf, p[2] >> 4, p[2] & 0xf));
                s.npixel_bytes = 0;
            }
            break;
        case 0x6:
            if (s.npixel_bytes == 3) {
                put_pixel(((p[0] & 0xf8) << 8) | ((p[1] & 0xfc) << 3) | (p[2] >> 3));
                s.npixel_bytes = 0;
            }
            break;
        default:
            if (s.npixel_bytes == 2) {
                put_pixel((p[0] << 8) | p[1]);
                s.npixel_bytes = 0;
            }
            break;
    }
}

static void command(uint8_t cmd) {
    s.cmd = cmd;
    s.nparams = 0;
    s.npixel_bytes = 0;
    switch (cmd) {
        case CMD_SWRESET:
            st7735_reset();
            break;
        case CMD_SLPIN:
        case CMD_SLPOUT:
            s.sleeping = cmd == CMD_SLPIN;
            changed();
            break;
        case CMD_INVOFF:
        case CMD_INVON:
            s.inverted = cmd == CMD_INVON;
            changed();
            break;
        case CMD_DISPOFF:
        case CMD_DISPON:
            s.display_on = cmd == CMD_DISPON;
            changed();
            break;
        case CMD_RAMWR:
            s.x = s.xs;
            s.y = s.ys;
            break;
        default:
            break;
    }
}

static void data(uint8_t b) {
    switch (s.cmd) {
        case CMD_RAMWR:
            pixel_byte(b);
            return;
        case CMD_CASET:
        case CMD_RASET:
            if (s.nparams < 4) {
                s.params[s.nparams++] = b;
            }
            if (s.nparams == 4) {
                const uint16_t start = (s.params[0] << 8) | s.params[1];
                const uint16_t end = (s.params[2] << 8) | s.params[3];
                if (s.cmd == CMD_CASET) {
                    s.xs = start;
                    s.xe = end;
                } else {
                    s.ys = start;
                    s.ye = end;
                }
            }
            return;
        case CMD_MADCTL:
            if (s.nparams++ == 0) {
                s.madctl = b;
                changed();
            }
            return;
        case CMD_COLMOD:
            if (s.nparams++ == 0) {
                s.colmod = b;
            }
            return;
        default:
            return;
    }
}

void st7735_write(bool dc, const uint8_t *bytes, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (dc) {
            data(bytes[i]);
        } else {
            command(bytes[i]);
        }
    }
}

uint32_t st7735_frame(void) {
    return s.frame;
}

const uint8_t *st7735_picture(void) {
    const bool blank = s.sleeping || !s.display_on;
    for (int py = 0; py < PANEL_H; py++) {
        for (int px = 0; px < PANEL_W; px++) {
            // rotation 270: panel pixel (px, py) -> picture pixel (ox, oy)
            const int ox = py, oy = PANEL_W - 1 - px;
            uint8_t *out = picture[oy][ox];
            if (blank) {
                out[0] = out[1] = out[2] = 0;
            } else {
                const uint16_t p = s.gram[py + Y_OFFSET][px + X_OFFSET];
                unsigned r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, b = p & 0x1f;
                if (s.madctl & MADCTL_BGR) {
                    unsigned t = r;
                    r = b;
                    b = t;
                }
                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);
                if (s.inverted) {
                    r ^= 0xff;
                    g ^= 0xff;
                    b ^= 0xff;
                }
                out[0] = r;
                out[1] = g;
                out[2] = b;
            }
            out[3] = 0xff;
        }
    }
    return &picture[0][0][0];
}
