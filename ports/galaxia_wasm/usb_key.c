// The Galaxia's USB key as the computer sees it (ports/esp32/msc_disk.c): the page
// reads and writes the FAT partition block by block, with its own FAT driver (a
// second oofatfs volume, as the computer's), while the firmware has it mounted. As on
// the board, writing block 1 (the FAT) restarts the program, which mounts the
// partition again. Each operation mounts the volume afresh: it sees what the
// firmware wrote.
//
// Called by worker.js between two steps of the firmware (the Worker runs one thing
// at a time); returns 0 or a byte count, or -FRESULT.

#include <string.h>
#include <emscripten.h>

#include "py/runtime.h"
#include "py/mperrno.h"
#include "extmod/vfs.h"
#include "extmod/vfs_fat.h"
#include "lib/oofatfs/ff.h"

extern uint8_t *galaxia_flash_image(void);
extern size_t galaxia_flash_size(void);
extern size_t galaxia_flash_block_size(void);
extern void galaxia_restart_program(void);

static fs_user_mount_t key;

static mp_uint_t key_read(uint8_t *buf, uint32_t block, uint32_t count) {
    size_t bs = galaxia_flash_block_size();
    if ((block + count) * bs > galaxia_flash_size()) {
        return -MP_EIO;
    }
    memcpy(buf, galaxia_flash_image() + block * bs, count * bs);
    return 0;
}

static mp_uint_t key_write(const uint8_t *buf, uint32_t block, uint32_t count) {
    size_t bs = galaxia_flash_block_size();
    if ((block + count) * bs > galaxia_flash_size()) {
        return -MP_EIO;
    }
    memcpy(galaxia_flash_image() + block * bs, buf, count * bs);
    if (block == 1) {
        galaxia_restart_program();      // as tud_msc_write10_cb()
    }
    return 0;
}

static mp_obj_t key_ioctl(mp_obj_t cmd_in, mp_obj_t arg_in) {
    (void)arg_in;
    switch (mp_obj_get_int(cmd_in)) {
        case MP_BLOCKDEV_IOCTL_BLOCK_COUNT:
            return MP_OBJ_NEW_SMALL_INT(galaxia_flash_size() / galaxia_flash_block_size());
        case MP_BLOCKDEV_IOCTL_BLOCK_SIZE:
            return MP_OBJ_NEW_SMALL_INT(galaxia_flash_block_size());
        default:
            return MP_OBJ_NEW_SMALL_INT(0);
    }
}
static MP_DEFINE_CONST_FUN_OBJ_2(key_ioctl_obj, key_ioctl);

static FRESULT key_mount(void) {
    memset(&key, 0, sizeof(key));
    key.base.type = &mp_fat_vfs_type;
    key.blockdev.flags = MP_BLOCKDEV_FLAG_NATIVE | MP_BLOCKDEV_FLAG_HAVE_IOCTL;
    key.blockdev.block_size = galaxia_flash_block_size();
    key.blockdev.readblocks[0] = MP_OBJ_FROM_PTR(&key_ioctl_obj);    // present
    key.blockdev.readblocks[2] = (mp_obj_t)(uintptr_t)key_read;
    key.blockdev.writeblocks[0] = MP_OBJ_FROM_PTR(&key_ioctl_obj);   // writable
    key.blockdev.writeblocks[2] = (mp_obj_t)(uintptr_t)key_write;
    key.blockdev.u.ioctl[0] = MP_OBJ_FROM_PTR(&key_ioctl_obj);
    key.blockdev.u.ioctl[1] = MP_OBJ_NULL;
    key.fatfs.drv = &key;
    return f_mount(&key.fatfs);
}

// The entries of a directory, one per line: "d\t0\tname" or "f\tsize\tname"
EMSCRIPTEN_KEEPALIVE
int galaxia_key_list(const char *path, char *out, int cap) {
    FRESULT res = key_mount();
    if (res != FR_OK) {
        return -res;
    }
    FF_DIR dir;
    res = f_opendir(&key.fatfs, &dir, path);
    if (res != FR_OK) {
        return -res;
    }
    int n = 0;
    FILINFO fno;
    while ((res = f_readdir(&dir, &fno)) == FR_OK && fno.fname[0]) {
        int w = snprintf(out + n, cap - n, "%c\t%u\t%s\n", (fno.fattrib & AM_DIR) ? 'd' : 'f',
            (unsigned)fno.fsize, fno.fname);
        if (w >= cap - n) {
            break;
        }
        n += w;
    }
    f_closedir(&dir);
    return res == FR_OK ? n : -res;
}

EMSCRIPTEN_KEEPALIVE
int galaxia_key_size(const char *path) {
    FRESULT res = key_mount();
    FILINFO fno;
    if (res == FR_OK) {
        res = f_stat(&key.fatfs, path, &fno);
    }
    return res == FR_OK ? (int)fno.fsize : -res;
}

EMSCRIPTEN_KEEPALIVE
int galaxia_key_read(const char *path, uint8_t *buf, int cap) {
    FRESULT res = key_mount();
    FIL f;
    if (res == FR_OK) {
        res = f_open(&key.fatfs, &f, path, FA_READ);
    }
    if (res != FR_OK) {
        return -res;
    }
    UINT n = 0;
    res = f_read(&f, buf, cap, &n);
    f_close(&f);
    return res == FR_OK ? (int)n : -res;
}

EMSCRIPTEN_KEEPALIVE
int galaxia_key_write(const char *path, const uint8_t *buf, int len) {
    FRESULT res = key_mount();
    FIL f;
    if (res == FR_OK) {
        res = f_open(&key.fatfs, &f, path, FA_WRITE | FA_CREATE_ALWAYS);
    }
    if (res != FR_OK) {
        return -res;
    }
    UINT n = 0;
    res = f_write(&f, buf, len, &n);
    FRESULT closed = f_close(&f);
    if (res == FR_OK && n != (UINT)len) {
        res = FR_DENIED;            // full
    }
    return res != FR_OK ? -res : closed != FR_OK ? -closed : (int)n;
}

EMSCRIPTEN_KEEPALIVE
int galaxia_key_mkdir(const char *path) {
    FRESULT res = key_mount();
    if (res == FR_OK) {
        res = f_mkdir(&key.fatfs, path);
    }
    return -res;
}

// A file, or an empty directory
EMSCRIPTEN_KEEPALIVE
int galaxia_key_remove(const char *path) {
    FRESULT res = key_mount();
    if (res == FR_OK) {
        res = f_unlink(&key.fatfs, path);
    }
    return -res;
}

// Free and total bytes
EMSCRIPTEN_KEEPALIVE
int galaxia_key_space(double *free_total) {
    FRESULT res = key_mount();
    DWORD nclst = 0;
    if (res == FR_OK) {
        res = f_getfree(&key.fatfs, &nclst);
    }
    if (res != FR_OK) {
        return -res;
    }
    double cluster = (double)key.fatfs.csize * galaxia_flash_block_size();
    free_total[0] = nclst * cluster;
    free_total[1] = (key.fatfs.n_fatent - 2) * cluster;
    return 0;
}
