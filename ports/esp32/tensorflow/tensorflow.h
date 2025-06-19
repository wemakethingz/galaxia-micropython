#ifndef MP_TENSORFLOW_H_
#define MP_TENSORFLOW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/objarray.h"

typedef struct _tensorflow_tensor_obj_t {
    mp_obj_base_t base;    
    mp_obj_t tf_tensor;
    // microlite_interpreter_obj_t *microlite_interpreter;
} tensorflow_tensor_obj_t;

#ifdef __cplusplus
}
#endif

#endif