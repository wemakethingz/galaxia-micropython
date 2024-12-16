/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Scott Shawcroft
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Microcontroller contains pin references and microcontroller specific control
// functions.

#include <stdint.h>

#include "py/obj.h"
#include "py/runtime.h"
#include "mphalport.h"
#include "shared/runtime/pyexec.h"

#include "common-thingz/thingz.h"




static mp_obj_t mcu_reset(void) {
    esp_restart();
    // We won't actually get here because we're resetting.
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mcu_reset_obj, mcu_reset);

static mp_obj_t enable_repl_flash(void) {
    esp_restart();
    // We won't actually get here because we're resetting.
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(enable_repl_flash_obj, enable_repl_flash);

static mp_obj_t set_file_to_execute(mp_obj_t filename) {
    
    const char* file = mp_obj_str_get_str(filename);

    thingz_set_python_file_to_exec(file);
    if(!pyexec_repl_active)
        mp_sched_reload_interrupt();
    else{
        ringbuf_put(&stdin_ringbuf, 4);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(set_file_to_execute_obj, set_file_to_execute);



static const mp_rom_map_elem_t mcu_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_microcontroller) },
    
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&mcu_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_enable_repl_flash), MP_ROM_PTR(&enable_repl_flash_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_file_to_execute), MP_ROM_PTR(&set_file_to_execute_obj) },

};

static MP_DEFINE_CONST_DICT(mcu_module_globals, mcu_module_globals_table);

const mp_obj_module_t microcontroller_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mcu_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_microcontroller, microcontroller_module);