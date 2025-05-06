/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2023 Damien P. George
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

#include "py/mpstate.h"

#ifdef MICROPY_THINGZ_DEBUG_MODE
#include "debug_mode/debug_mode.h"
#include "obj.h"
#endif

#if MICROPY_NLR_SETJMP

void nlr_jump(void *val) {
    #ifdef MICROPY_THINGZ_DEBUG_MODE

    void *_val = MP_OBJ_TO_PTR(val);
    if(_val != NULL){
        if(mp_obj_is_exception_instance(val)){
            const mp_obj_type_t *type = mp_obj_get_type(val);
            debug_mode_reset_last_exception();
            if(type != &mp_type_ReloadInterrupt){
                if(type != &mp_type_KeyboardInterrupt)
                    mp_obj_print_exception(&thgz_debug_exception_print, val);
            }else{
                debug_mode_stop();
            }
        }
    }
    #endif
    MP_NLR_JUMP_HEAD(val, top);
    longjmp(top->jmpbuf, 1);
}

#endif
