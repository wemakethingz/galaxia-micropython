#include <stdio.h>
#include <stdlib.h>
#include "py/runtime.h"
#include "py/obj.h"

#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

#include "tensorflow_interface.h"

#define TENSOR_ARENA_SIZE (2 * 1024)
static uint8_t tensor_arena[TENSOR_ARENA_SIZE];

extern "C"{

uint8_t tensorflow_run_from_file(uint8_t* modelData, size_t modelSize, float in, float* out) {
    const tflite::Model *model = tflite::GetModel(modelData);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        return MP_TENSORFLOW_RUN_FROM_FILE_SCHEMA_VERSION_MISMATCH;
    }

    tflite::MicroMutableOpResolver<1> resolver;
    if (resolver.AddFullyConnected() != kTfLiteOk) {
        return MP_TENSORFLOW_RUN_FROM_FILE_RESOLVER_NOT_CONNECTED;
    }
    tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, TENSOR_ARENA_SIZE);
    interpreter.AllocateTensors();

    TfLiteTensor *input = interpreter.input(0);
    input->data.int8[0] = in / input->params.scale + input->params.zero_point;
    mp_printf(MP_PYTHON_PRINTER, "IN: %f, zero %d scale %d res %d\n", in, input->params.zero_point, input->params.scale, input->data.int8[0]);
    if (interpreter.Invoke() != kTfLiteOk) {
        return MP_TENSORFLOW_RUN_FROM_FILE_INFERENCE_FAILED;
    }

    TfLiteTensor *output = interpreter.output(0);
    *out = (output->data.int8[0] - output->params.zero_point) * output->params.scale;
    mp_printf(MP_PYTHON_PRINTER, "OUT: %d, zero %d scale %d res %f\n", output->data.int8[0], output->params.zero_point, output->params.scale, *out);
    // mp_obj_t result = mp_obj_new_list(output->dims->data[1], NULL);
    // for (int i = 0; i < output->dims->data[1]; ++i) {
    //     mp_obj_list_store(result, mp_obj_new_int(i), mp_obj_new_int(output->data.int8[i]));
    // }

    // return result;
    return 0;
}

}