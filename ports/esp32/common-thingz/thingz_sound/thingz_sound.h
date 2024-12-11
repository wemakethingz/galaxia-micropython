#ifndef THINGZ_SOUND_H_
#define THINGZ_SOUND_H_

#include "py/obj.h"
#include "py/reader.h"
#include "extmod/vfs.h"
#include "driver/dac_continuous.h"

extern const mp_obj_type_t thingz_sound_type;

typedef struct {
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t  format;
    uint32_t subchunk1_id;
    uint32_t subchunk1_size;
    uint16_t  audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t bitrate;
    uint16_t block_align;
    uint16_t bit_per_sample;
    uint32_t subchunk2_id;
    uint32_t subchunk2_size;
    uint64_t currentPos;
} thingz_wav_t;

typedef struct {
	mp_obj_base_t base;
    int8_t pinJackL;
    int8_t pinJackR;
    int8_t pinJackGND;
    float current_freq;
    float requested_freq;
    bool state;
    bool init;
    uint8_t current_volume;
    uint8_t request_volume;
    uint8_t mode;
    mp_reader_t reader;
    thingz_wav_t wav;
    dac_continuous_handle_t dac_handle;
} thingz_sound_obj_t;


void thingz_sound_init(thingz_sound_obj_t* sound, int8_t pinJackL, int8_t pinJackR, int8_t pinJackGND);
void thingz_sound_deinit(thingz_sound_obj_t* sound);
#endif