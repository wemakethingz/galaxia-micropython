#include <stdio.h>
#include <stdlib.h>
#include "py/runtime.h"
#include "py/obj.h"
#include "tensorflow.h"
#include "extmod/vfs.h"
#include "py/stream.h"
#include "string.h"

#include "tensorflow/lite/c/common.h"
#include "tensorflow_interface.h"
#include "tensorflow.h"

// #include "tensorflow/lite/schema/schema_generated.h"
// #include "tensorflow/lite/micro/micro_interpreter.h"
// #include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

// #define TENSOR_ARENA_SIZE (2 * 1024)
// static uint8_t tensor_arena[TENSOR_ARENA_SIZE];
const mp_obj_type_t tensorflow_interpreter_type;
const mp_obj_type_t tensorflow_tensor_type;

static mp_obj_t interpreter_get_input_tensor(mp_obj_t self_in, mp_obj_t index_obj);


static void tensor_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    tensorflow_tensor_obj_t *self = MP_OBJ_TO_PTR(self_in);

    TfLiteTensor * tensor = (TfLiteTensor *)self->tf_tensor;

    mp_print_str(print, "tensor(type=");

    mp_print_str(print, TfLiteTypeGetName(tensor->type));

    int size = tensor->dims->size;

    mp_printf(print, ", dims->size=%d\n", size);

    mp_print_str(print, ")\n");
}

static mp_obj_t tensor_get_tensor_type (mp_obj_t self_in, mp_obj_t index_obj) {

    tensorflow_tensor_obj_t *self = MP_OBJ_TO_PTR(self_in);

    TfLiteTensor * tensor = (TfLiteTensor *)self->tf_tensor;

    char *type = TfLiteTypeGetName(tensor->type);

    return mp_obj_new_str(type, strlen(type));

}

MP_DEFINE_CONST_FUN_OBJ_2(tensor_tensor_get_tensor_type, tensor_get_tensor_type);


static mp_obj_t tensor_get_value (mp_obj_t self_in, mp_obj_t index_obj) {

    tensorflow_tensor_obj_t *self = MP_OBJ_TO_PTR(self_in);

    TfLiteTensor * tensor = (TfLiteTensor *)self->tf_tensor;

    if (tensor->type == kTfLiteFloat32) {
        mp_int_t index = mp_obj_int_get_checked(index_obj);

        float f_value = tensor->data.f[index];

        return mp_obj_new_float_from_f(f_value);
    }
    else if (tensor->type == kTfLiteInt8) {
        mp_int_t index = mp_obj_int_get_checked(index_obj);

        int8_t int8_value = tensor->data.int8[index];

        return mp_obj_new_int(int8_value);
    }
     else if (tensor->type == kTfLiteUInt8) {
        mp_int_t index = mp_obj_int_get_checked(index_obj);

        uint8_t int8_value = tensor->data.uint8[index];

        return mp_obj_new_int(int8_value);
    }
    else {
        mp_raise_TypeError("Unsupported Tensor Type");
    }

    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_2(tensor_tensor_get_value, tensor_get_value);

static mp_obj_t tensor_set_value (mp_obj_t self_in, mp_obj_t index_obj, mp_obj_t value) {
    
    mp_int_t index = mp_obj_int_get_checked(index_obj);

    tensorflow_tensor_obj_t *self = MP_OBJ_TO_PTR(self_in);

    TfLiteTensor * tensor = (TfLiteTensor *)self->tf_tensor;

    if (tensor->type == kTfLiteFloat32) {
        tensor->data.f[index] = mp_obj_get_float_to_f(value);
    }
    else if (tensor->type == kTfLiteInt8) {
        mp_int_t int_value = mp_obj_int_get_checked(value);

        int8_t int8_value = (int8_t)int_value;

        tensor->data.int8[index] = int8_value;
    }
    else if (tensor->type == kTfLiteUInt8) {
        mp_int_t int_value = mp_obj_int_get_checked(value);

        uint8_t uint8_value = (uint8_t)int_value;

        tensor->data.uint8[index] = uint8_value;
    }
    else {
        mp_raise_TypeError("Unsupported Tensor Type");
    }

    return MP_OBJ_FROM_PTR(self);
}

MP_DEFINE_CONST_FUN_OBJ_3(tensor_tensor_set_value, tensor_set_value);

static mp_obj_t tensor_quantize_float_to_int8 (mp_obj_t self_in, mp_obj_t float_obj) {
    
    if (!mp_obj_is_float(float_obj)) {
         mp_raise_TypeError("Expecting Parameter of float type");
        // return mp_const_none;
    }

    tensorflow_tensor_obj_t *self = MP_OBJ_TO_PTR(self_in);

    TfLiteTensor * tensor = (TfLiteTensor *)self->tf_tensor;

    if (tensor->type != kTfLiteInt8) {
        mp_raise_TypeError ("Expected Tensor to be of type ktfLiteInt8.");
    }

    // may need to add #ifdef sheild on this method as its only defined on boards with floating point
    float value = mp_obj_get_float_to_f(float_obj);

    // Quantize the input from floating-point to integer
    int8_t quantized_value = (int8_t)(value / tensor->params.scale + tensor->params.zero_point);

    return MP_OBJ_NEW_SMALL_INT(quantized_value);
}

MP_DEFINE_CONST_FUN_OBJ_2(tensor_tensor_quantize_float_to_int8, tensor_quantize_float_to_int8);

static mp_obj_t tensor_quantize_int8_to_float (mp_obj_t self_in, mp_obj_t int_obj) {
    
    if (!mp_obj_is_integer(int_obj)) {
         mp_raise_TypeError("Expecting Parameter of float type");
        // return mp_const_none;
    }

    tensorflow_tensor_obj_t *self = MP_OBJ_TO_PTR(self_in);

    TfLiteTensor * tensor = (TfLiteTensor *)self->tf_tensor;

    if (tensor->type != kTfLiteInt8) {
        mp_raise_TypeError ("Expected Tensor to be of type ktfLiteInt8.");
    }

    // may need to add #ifdef sheild on this method as its only defined on boards with floating point
    int8_t value = mp_obj_int_get_checked(int_obj);

    // Quantize the input from floating-point to integer
    float quantized_value = (value - tensor->params.zero_point) * tensor->params.scale;

    return mp_obj_new_float_from_f(quantized_value);
}

MP_DEFINE_CONST_FUN_OBJ_2(tensor_tensor_quantize_int8_to_float, tensor_quantize_int8_to_float);

// interpreter class
static const mp_rom_map_elem_t tensor_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_getValue), MP_ROM_PTR(&tensor_tensor_get_value) },
    { MP_ROM_QSTR(MP_QSTR_setValue), MP_ROM_PTR(&tensor_tensor_set_value) },
    { MP_ROM_QSTR(MP_QSTR_getType), MP_ROM_PTR(&tensor_tensor_get_tensor_type) },
    { MP_ROM_QSTR(MP_QSTR_quantizeFloatToInt8), MP_ROM_PTR(&tensor_tensor_quantize_float_to_int8) },
    { MP_ROM_QSTR(MP_QSTR_quantizeInt8ToFloat), MP_ROM_PTR(&tensor_tensor_quantize_int8_to_float) }
};

static MP_DEFINE_CONST_DICT(tensor_locals_dict, tensor_locals_dict_table);

const mp_obj_type_t microlite_tensor_type = {
    { &mp_type_type },
    .name = MP_QSTR_tensor,
    .print = tensor_print,
    .locals_dict = (mp_obj_dict_t*)&tensor_locals_dict,
};

static mp_obj_t tflm_run_from_file(mp_obj_t file_path_obj, mp_obj_t input) {
    const char *filename = mp_obj_str_get_str(file_path_obj);
    mp_obj_t args[] = {
        mp_obj_new_str(filename, strlen(filename)),
        mp_obj_new_str("rb", 2),
    };
    mp_obj_t f = mp_vfs_open(2, args, &mp_const_empty_map);
        
    if (f == mp_const_none) {
        mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("Cannot open file: %s"), filename);

    }
    int err;
    mp_stream_seek(f, 0, MP_SEEK_END, &err);
    mp_off_t offset = mp_stream_seek(f, 0, MP_SEEK_CUR, &err);
    size_t model_size = offset+1;
    mp_stream_seek(f, 0, MP_SEEK_SET, &err);

    uint8_t *model_data = (uint8_t *)m_malloc(model_size);
    if (!model_data) {
        mp_stream_close(f);
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("Memory allocation failed for model"));
    }

    mp_stream_read_exactly(f, model_data, model_size, &err);
    mp_stream_close(f);
    mp_printf(MP_PYTHON_PRINTER, "Model size: %d\n", model_size);

    float in = mp_obj_get_float(input);
    float out;
    uint8_t res = tensorflow_run_from_file(model_data, model_size, in, &out);
    m_free(model_data);
    mp_printf(MP_PYTHON_PRINTER, "Done result: %d\n", res);
    if(res == MP_TENSORFLOW_RUN_FROM_FILE_CANT_OPEN){
        mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("Cannot open file: %s"), filename);
    }else if(res == MP_TENSORFLOW_RUN_FROM_FILE_MODEL_ALLOCATION_FAILED){
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("Memory allocation failed for model"));
    }else if( res == MP_TENSORFLOW_RUN_FROM_FILE_SCHEMA_VERSION_MISMATCH){
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Model schema version mismatch"));
    }else if( res == MP_TENSORFLOW_RUN_FROM_FILE_RESOLVER_NOT_CONNECTED){
        mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("Resolver not fully connected"));
    }else if(res == MP_TENSORFLOW_RUN_FROM_FILE_INFERENCE_FAILED){
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Inference failed"));
    }
    return mp_obj_new_float(out);
}

MP_DEFINE_CONST_FUN_OBJ_2(tflm_run_from_file_obj, tflm_run_from_file);
static const mp_map_elem_t tflm_module_globals_table[] = {
    {MP_OBJ_NEW_QSTR(MP_QSTR_run_from_file), (mp_obj_t)&tflm_run_from_file_obj},
};

static MP_DEFINE_CONST_DICT(tflm_module_globals, tflm_module_globals_table);

const mp_obj_module_t tflm_user_cmodule = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&tflm_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_tensorflow, tflm_user_cmodule);


