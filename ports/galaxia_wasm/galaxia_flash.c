// The Galaxia's FAT partition (the "USB key"), in memory, as a block device for
// vfs.VfsFat: _galaxia.flash. Its storage is outside the MicroPython heap, so the
// files survive soft resets as on the board's flash. The page can read and write
// the image through galaxia_flash_image() / galaxia_flash_size().

#include <string.h>
#include <emscripten.h>

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/obj.h"
#include "extmod/vfs.h"
#include "lib/oofatfs/ff.h"

#define BLOCK_SIZE 512
#define BLOCK_COUNT 4032        // ~2 MB, as MICROPY_HW_FLASH_STORAGE_BYTES on the board

static uint8_t *storage;

EMSCRIPTEN_KEEPALIVE
uint8_t *galaxia_flash_image(void) {
    if (!storage) {
        storage = calloc(BLOCK_COUNT, BLOCK_SIZE);
    }
    return storage;
}

EMSCRIPTEN_KEEPALIVE
size_t galaxia_flash_size(void) {
    return (size_t)BLOCK_COUNT * BLOCK_SIZE;
}

typedef struct {
    mp_obj_base_t base;
} galaxia_flash_obj_t;

static void check_range(mp_int_t block, size_t offset, size_t len) {
    if (block < 0 || (block * BLOCK_SIZE + offset + len) > galaxia_flash_size()) {
        mp_raise_OSError(MP_EIO);
    }
}

static mp_obj_t flash_readblocks(size_t n_args, const mp_obj_t *args) {
    mp_int_t block = mp_obj_get_int(args[1]);
    mp_buffer_info_t buf;
    mp_get_buffer_raise(args[2], &buf, MP_BUFFER_WRITE);
    size_t offset = n_args > 3 ? mp_obj_get_int(args[3]) : 0;
    check_range(block, offset, buf.len);
    memcpy(buf.buf, galaxia_flash_image() + block * BLOCK_SIZE + offset, buf.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flash_readblocks_obj, 3, 4, flash_readblocks);

static mp_obj_t flash_writeblocks(size_t n_args, const mp_obj_t *args) {
    mp_int_t block = mp_obj_get_int(args[1]);
    mp_buffer_info_t buf;
    mp_get_buffer_raise(args[2], &buf, MP_BUFFER_READ);
    size_t offset = n_args > 3 ? mp_obj_get_int(args[3]) : 0;
    check_range(block, offset, buf.len);
    memcpy(galaxia_flash_image() + block * BLOCK_SIZE + offset, buf.buf, buf.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flash_writeblocks_obj, 3, 4, flash_writeblocks);

static mp_obj_t flash_ioctl(mp_obj_t self, mp_obj_t op_in, mp_obj_t arg) {
    switch (mp_obj_get_int(op_in)) {
        case MP_BLOCKDEV_IOCTL_BLOCK_COUNT:
            return MP_OBJ_NEW_SMALL_INT(BLOCK_COUNT);
        case MP_BLOCKDEV_IOCTL_BLOCK_SIZE:
            return MP_OBJ_NEW_SMALL_INT(BLOCK_SIZE);
        default:
            return MP_OBJ_NEW_SMALL_INT(0);
    }
}
static MP_DEFINE_CONST_FUN_OBJ_3(flash_ioctl_obj, flash_ioctl);

static const mp_rom_map_elem_t flash_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_readblocks), MP_ROM_PTR(&flash_readblocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_writeblocks), MP_ROM_PTR(&flash_writeblocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&flash_ioctl_obj) },
};
static MP_DEFINE_CONST_DICT(flash_locals_dict, flash_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    galaxia_flash_type,
    MP_QSTR_Flash,
    MP_TYPE_FLAG_NONE,
    locals_dict, &flash_locals_dict
    );

static const galaxia_flash_obj_t galaxia_flash_obj = { { &galaxia_flash_type } };

static const mp_rom_map_elem_t galaxia_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR__galaxia) },
    { MP_ROM_QSTR(MP_QSTR_flash), MP_ROM_PTR(&galaxia_flash_obj) },
};
static MP_DEFINE_CONST_DICT(galaxia_module_globals, galaxia_module_globals_table);

const mp_obj_module_t galaxia_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&galaxia_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR__galaxia, galaxia_module);

// FatFs time stamps
DWORD get_fattime(void) {
    return ((2026 - 1980) << 25) | (1 << 21) | (1 << 16);
}
