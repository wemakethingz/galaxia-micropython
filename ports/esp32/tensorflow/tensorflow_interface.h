#ifndef MP_TENSORFLOW_INTERFACE_H_
#define MP_TENSORFLOW_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

#define MP_TENSORFLOW_RUN_FROM_FILE_CANT_OPEN 1
#define MP_TENSORFLOW_RUN_FROM_FILE_MODEL_ALLOCATION_FAILED 2
#define MP_TENSORFLOW_RUN_FROM_FILE_SCHEMA_VERSION_MISMATCH 3
#define MP_TENSORFLOW_RUN_FROM_FILE_RESOLVER_NOT_CONNECTED 4
#define MP_TENSORFLOW_RUN_FROM_FILE_INFERENCE_FAILED 5

uint8_t tensorflow_run_from_file(uint8_t* modelData, size_t modelSize, float in, float* out);

#ifdef __cplusplus
}
#endif

#endif