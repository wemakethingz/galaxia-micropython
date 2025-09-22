#include "thingz_sound.h"
#include "thingz_sound_sample.h"

#include "py/runtime.h"
#include "py/objstr.h"
#include "py/stream.h"
#include "py/qstr.h"

#include "driver/gpio.h"
#include "driver/adc.h"

#include "esp_timer.h"

#include "soc/soc.h"
#include "soc/spi_reg.h"
#include "soc/spi_struct.h"
#include "soc/system_reg.h"
#include "soc/periph_defs.h"

#include "driver/dac_continuous.h"

#include "soc/lldesc.h" 


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>


#define THINGZ_SOUND_FILE_READ_STATE_STARTED 0
#define THINGZ_SOUND_FILE_READ_STATE_LAST_BLOCK 1
#define THINGZ_SOUND_FILE_READ_STATE_ENDED 2

#define THINGZ_SOUND_SINUS_NOT_PLAYING 0
#define THINGZ_SOUND_SINUS_PLAYING 1

#define THINGZ_SOUND_MODE_NOT_INIT 0
#define THINGZ_SOUND_MODE_SINUS 1
#define THINGZ_SOUND_MODE_WAV 2

#define THINGZ_SOUND_WAVE_LEN 400
#define THINGZ_SOUND_SAMPLE_WAV_LEN 1024
#define CONST_PERIOD_2_PI           6.2832                  // 2 * PI

static uint8_t thingz_sin_wave[THINGZ_SOUND_WAVE_LEN];
static uint8_t thingz_sample_wav[THINGZ_SOUND_SAMPLE_WAV_LEN];                      // Used to store sine wave values
                      // Used to store sine wave values
// uint8_t tri_wav[EXAMPLE_ARRAY_LEN];                      // Used to store triangle wave values
// uint8_t saw_wav[EXAMPLE_ARRAY_LEN];                      // Used to store sawtooth wave values
// uint8_t squ_wav[EXAMPLE_ARRAY_LEN];                      // Used to store square wave values

static void thingz_sound_generate_wave(uint8_t volume)
{
    uint32_t pnt_num = THINGZ_SOUND_WAVE_LEN;
    double amplitude = (double)((volume/100.0f)*255.0f);
    for (int i = 0; i < pnt_num; i ++) {
        thingz_sin_wave[i] = (uint8_t)((sin(i * CONST_PERIOD_2_PI / pnt_num) + 1) * (double)(amplitude) / 2 + 0.5);
        // tri_wav[i] = (i > (pnt_num / 2)) ? (2 * EXAMPLE_DAC_AMPLITUDE * (pnt_num - i) / pnt_num) : (2 * EXAMPLE_DAC_AMPLITUDE * i / pnt_num);
        // saw_wav[i] = (i == pnt_num) ? 0 : (i * EXAMPLE_DAC_AMPLITUDE / pnt_num);
        // squ_wav[i] = (i < (pnt_num / 2)) ? EXAMPLE_DAC_AMPLITUDE : 0;
    }

    // for(int i = 0; i < pnt_num; i++){ 
    //     thingz_sin_wave[i] = (uint8_t)roundf((double)volume/(double)100.0*((double)95*((double)1.0+(double)sinf((double)M_PI*(double)2.0*(double)(i/((double)pnt_num-1))))));
    //     //printf("buf %d\n", out->buf[i]);
    // }
}

static uint8_t thingz_sound_copy_sample_bytes(thingz_sound_obj_t *sound, uint8_t* buffer, uint32_t*bytesRead){
    if(!buffer)
        return 1;

    uint32_t i;
    for(i = 0; i < THINGZ_SOUND_SAMPLE_WAV_LEN; i++){
        mp_uint_t b = sound->reader.readbyte(sound->reader.data);
        mp_uint_t value;
        
        if (b == MP_READER_EOF) {
            return 1;
        }

        if(sound->wav.num_channels == 1){
            value = b;
        }else{
            value = b;
            b = sound->reader.readbyte(sound->reader.data);
            value = (value+b)/2;
        }
        
        buffer[i] = (uint8_t)roundf(((double)sound->current_volume/(double)100.0)*value);
        //if stereo second byte return eof
        if (b == MP_READER_EOF) {
            return 1;
        }
    }
    *bytesRead = i;
    return 0;
}

static void thingz_sound_wav_decode(thingz_sound_obj_t * sound){
    uint8_t header[44];
    int i;

    for(i = 0; i < 44; i++){
        mp_uint_t b = sound->reader.readbyte(sound->reader.data);
        // mp_printf(MP_PYTHON_PRINTER, "isr %d i %d\n", b, i);

        if (b == MP_READER_EOF){
            mp_raise_TypeError("File is not in WAV format");
        }
        header[i] = b;
    }
    memcpy(&sound->wav, header, 44);

    if(sound->wav.chunk_id != 0x46464952){
        // mp_printf(MP_PYTHON_PRINTER, "%d", sound->wav.chunk_id);
        mp_raise_ValueError("Wrong chunk id, file is not correct wav");
    }

    if(sound->wav.format != 0x45564157){
        mp_raise_ValueError("Wrong format, file is not correct wav");
    }

    if(sound->wav.subchunk1_id != 0x20746d66){
        mp_raise_ValueError("Wrong sub chunk id, file is not correct wav");
    }

    if(sound->wav.audio_format != 1){
        mp_raise_ValueError("Unsupported format, only PCM is supported");
    }

    if(sound->wav.num_channels > 2){
        mp_raise_ValueError("Only mono and stereo files are supported");
    }

    if(sound->wav.bit_per_sample != 8){
        mp_raise_ValueError("Only 8 bits sample are supported");
    }

    sound->wav.bitrate = sound->wav.bitrate / sound->wav.num_channels;
}

static void thingz_sound_set_ios(uint8_t on, int8_t jackL, int8_t jackR, int8_t jackGND){
    gpio_config_t io_conf;
    io_conf.pin_bit_mask = 1ull << jackGND;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    gpio_reset_pin(jackGND);

    gpio_config(&io_conf);

    if(on){
        gpio_set_level(jackGND,0);
        gpio_set_drive_capability(jackGND, GPIO_DRIVE_CAP_3);
        
        gpio_set_level(jackGND, 0);
    }else{
        gpio_set_level(jackGND,0);
        gpio_set_level(jackR,0);
        gpio_set_level(jackL,0);
        gpio_set_drive_capability(jackGND, GPIO_DRIVE_CAP_3);
    }
}

static void thingz_sound_config_sinus(thingz_sound_obj_t* sound){
    sound->mode = THINGZ_SOUND_MODE_SINUS;
    if(sound->current_freq == 0){
        return;
    }
    if(sound->dac_handle){
        dac_continuous_disable(sound->dac_handle);
        dac_continuous_del_channels(sound->dac_handle);
    }
    dac_continuous_config_t cont_cfg = {
        .chan_mask = DAC_CHANNEL_MASK_ALL,
        .desc_num = 8,
        .buf_size = 128,
        .freq_hz = THINGZ_SOUND_WAVE_LEN*sound->current_freq,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_APLL,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };
    
    /* Allocate continuous channels */
    dac_continuous_new_channels(&cont_cfg, &sound->dac_handle);
    if(sound->current_volume != sound->request_volume){
        sound->current_volume = sound->request_volume;
        thingz_sound_generate_wave(sound->current_volume);
    }

    

}

void thingz_sound_init(thingz_sound_obj_t* sound, int8_t pinJackL, int8_t pinJackR, int8_t pinJackGND){
    
    int i;

    sound->pinJackL = pinJackL;
    sound->pinJackR = pinJackR;
    sound->pinJackGND = pinJackGND;

    sound->base.type = &thingz_sound_type;

    sound->current_freq = -1;
    sound->requested_freq = -1;
    sound->state = 0;
    sound->mode = THINGZ_SOUND_MODE_NOT_INIT;

    sound->current_volume = 0;
    sound->request_volume = 5;

    sound->dac_handle = NULL;

    #if defined(DEBUG) || defined(ENABLE_JTAG)
        //JTAG share pins with jack mass
        return;
    #endif

    sound->init = true;
}

void thingz_sound_deinit(thingz_sound_obj_t* sound){
    #if defined(DEBUG) || defined(ENABLE_JTAG)
        //JTAG share pins with sound
        return;
    #endif
    if(sound->init == false)
        return;

    if(sound->dac_handle){
        dac_continuous_del_channels(sound->dac_handle);
    }
    sound->init = false;
    
}

static void thingz_sound_play(thingz_sound_obj_t* sound, uint8_t on){
    
    if(sound->mode != THINGZ_SOUND_MODE_SINUS
    || sound->current_freq != sound->requested_freq
    || sound->current_volume != sound->request_volume
    ){
        //Regenerate DMA config

        sound->current_freq = sound->requested_freq;
        thingz_sound_config_sinus(sound);
        if(on && sound->current_freq > 0){
            thingz_sound_set_ios(1, sound->pinJackL, sound->pinJackR, sound->pinJackGND);
            dac_continuous_enable(sound->dac_handle);
            dac_continuous_write_cyclically(sound->dac_handle, (uint8_t *)thingz_sin_wave, THINGZ_SOUND_WAVE_LEN, NULL);
        }
    }
    
    if(!on || sound->current_freq == 0){
        thingz_sound_set_ios(0, sound->pinJackL, sound->pinJackR, sound->pinJackGND);
        dac_continuous_disable(sound->dac_handle);
        sound->current_freq = 0;    
    }
}




static void thingz_play_sample(thingz_sound_obj_t* sound){
    
    if(sound->dac_handle){
        dac_continuous_disable(sound->dac_handle);
        dac_continuous_del_channels(sound->dac_handle);
    }

    if(sound->request_volume || sound->current_volume){
        sound->current_volume = sound->request_volume;
    }

    dac_continuous_config_t cont_cfg = {
        .chan_mask = DAC_CHANNEL_MASK_ALL,
        .desc_num = 3,
        .buf_size = THINGZ_SOUND_SAMPLE_WAV_LEN,
        .freq_hz = sound->wav.bitrate,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_APLL,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };

    thingz_sound_set_ios(1, sound->pinJackL, sound->pinJackR, sound->pinJackGND);

    /* Allocate continuous channels */
    dac_continuous_new_channels(&cont_cfg, &sound->dac_handle);
   
    dac_continuous_enable(sound->dac_handle);
    sound->mode = THINGZ_SOUND_MODE_WAV;
    uint32_t bytesOut;
    while (1){
        bytesOut = 0;
        uint8_t ret = thingz_sound_copy_sample_bytes(sound, thingz_sample_wav, &bytesOut);
        
        if(bytesOut)
            dac_continuous_write(sound->dac_handle, thingz_sample_wav, bytesOut, NULL, -1);
        if(ret){
            break;
        }
    }

    dac_continuous_disable(sound->dac_handle);
    thingz_sound_set_ios(0, sound->pinJackL, sound->pinJackR, sound->pinJackGND);

    
}
//|
//|
//| """ Thingz Sound
//| """
//|
//| class Sound:
//|    """Output sound using Galaxia's Jack connector"""
//|
//|
//NEW
static mp_obj_t mp_thingz_sound_make_new(const mp_obj_type_t *type,
        mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    thingz_sound_obj_t *self = m_malloc_with_finaliser(sizeof(thingz_sound_obj_t));
    
    self->base.type = &thingz_sound_type;

    return (mp_obj_t)self;
}

//DEL
static mp_obj_t mp_thingz_sound_del(mp_obj_t self_in) {
	
	return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_sound_del_obj, mp_thingz_sound_del);


//PLAY
//|    def play(self, on: bool, freq: int) -> None:
//|        """
//|        :param bool on: Enable frequency generation onto the jack
//|        :param int freq: The frequency to generate in Hz
//|        """
//|        ...
//|
static const mp_arg_t mp_thingz_sound_play_args[] = {
    { MP_QSTR_on,      MP_ARG_REQUIRED | MP_ARG_BOOL, {.u_obj = mp_const_none}},
    { MP_QSTR_freq,    MP_ARG_INT, {.u_int = 440}},
};
#define MP_THINGZ_SOUND_PLAY_NUM_ARGS MP_ARRAY_SIZE(mp_thingz_sound_play_args)

static mp_obj_t mp_thingz_sound_play(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {

    #if defined(DEBUG) || defined(ENABLE_JTAG)
        //JTAG share pins with sound
        return mp_const_none;
    #endif
    thingz_sound_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    if(self->init && (self->pinJackL != 17 || self->pinJackR != 18))
        return mp_const_none;

    // parse args
    mp_arg_val_t vals[MP_THINGZ_SOUND_PLAY_NUM_ARGS];
    mp_arg_parse_all(n_args - 1, args + 1, kw_args, MP_THINGZ_SOUND_PLAY_NUM_ARGS, mp_thingz_sound_play_args, vals);

    uint8_t on = vals[0].u_bool;
    self->requested_freq = vals[1].u_int;
    thingz_sound_play(self, on);
        
    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_sound_play_obj, 2, mp_thingz_sound_play);

//SET FREQUENCY
//|    def set_frequency(self, freq: int) -> None:
//|        """
//|        :param int freq: The frequency to generate in Hz
//|        """
//|        ...
//|
static const mp_arg_t mp_thingz_sound_set_freq_args[] = {
    { MP_QSTR_freq,      MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = mp_const_none}},
};
#define MP_THINGZ_SOUND_SET_FREQ_NUM_ARGS MP_ARRAY_SIZE(mp_thingz_sound_set_freq_args)

static mp_obj_t mp_thingz_sound_set_freq(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    #if defined(DEBUG) || defined(ENABLE_JTAG)
        //JTAG share pins with sound
        return mp_const_none;
    #endif
    thingz_sound_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    if(self->init && (self->pinJackL != 17 || self->pinJackR != 18))
        return mp_const_none;

    // parse args
    mp_arg_val_t vals[MP_THINGZ_SOUND_SET_FREQ_NUM_ARGS];
    mp_arg_parse_all(n_args - 1, args + 1, kw_args, MP_THINGZ_SOUND_SET_FREQ_NUM_ARGS, mp_thingz_sound_set_freq_args, vals);

    float freq, oldFreq;

    if(mp_obj_is_float(vals[0].u_obj)){
        freq = mp_obj_get_float(vals[0].u_obj);
    }else{
        freq = mp_obj_get_int(vals[0].u_obj);
    }

    self->requested_freq = freq;
    thingz_sound_play(self, true);

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_sound_set_freq_obj, 2, mp_thingz_sound_set_freq);

//SET VOLUME
//|    def set_volume(self, volume: int) -> None:
//|        """
//|        Set the volume of the sound
//|
//|        :param int volume: The volume of the sound between 0 and 100
//|        """
//|        ...
//|
static mp_obj_t mp_thingz_sound_set_volume(mp_obj_t self_in, mp_obj_t volume) {

    thingz_sound_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    mp_int_t v;
    if(!mp_obj_get_int_maybe(volume, &v)){
        mp_raise_TypeError("argument 'volume' must be int");
    }
    if(v < 0 || v > 100){
        mp_raise_ValueError("argument 'volume' must be between 0 and 100");
    }

    self->request_volume = v;
    if(self->mode == THINGZ_SOUND_MODE_SINUS)
        thingz_sound_play(self, self->state);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_thingz_sound_set_volume_obj, mp_thingz_sound_set_volume);

//deinit
static mp_obj_t mp_thingz_sound_deinit(mp_obj_t self_in) {

    thingz_sound_obj_t *self = MP_OBJ_TO_PTR(self_in);
    
    
    thingz_sound_deinit(self);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_thingz_sound_deinit_obj, mp_thingz_sound_deinit);


//|    def play_sample(self, filename: str) -> None:
//|        """
//|        Play a sound sample. Sample must be in wav format
//|
//|        :param str filename: The path to file
//|        """
//|        ...
//|
static const mp_arg_t mp_thingz_sound_play_sample_args[] = {
    { MP_QSTR_filename,      MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = mp_const_none}},
};
#define MP_THINGZ_SOUND_PLAY_SAMPLE_NUM_ARGS MP_ARRAY_SIZE(mp_thingz_sound_play_sample_args)

static mp_obj_t mp_thingz_sound_play_sample(uint n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    thingz_sound_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    if(self->pinJackL != 17 || self->pinJackR != 18)
        return mp_const_none;

    // parse args
    mp_arg_val_t vals[MP_THINGZ_SOUND_PLAY_SAMPLE_NUM_ARGS];
    mp_arg_parse_all(n_args - 1, args + 1, kw_args, MP_THINGZ_SOUND_PLAY_SAMPLE_NUM_ARGS, mp_thingz_sound_play_sample_args, vals);

    gpio_config_t io_conf;

    const char *filename = mp_obj_str_get_str(vals[0].u_obj);
    printf("FILENAME %s\n", filename);
    mp_reader_new_file(&self->reader, qstr_from_str(filename));

    thingz_sound_wav_decode(self);
    // return mp_const_none;
    // uint8_t buff[1000];
    // int err;
    // mp_stream_read_exactly(file, buff, 1000, &err);
    //mp_stream_read_obj.fun.var(2, file_args);
    
    thingz_play_sample(self);

    self->reader.close(self->reader.data);

    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_KW(mp_thingz_sound_play_sample_obj, 1, mp_thingz_sound_play_sample);

static const mp_map_elem_t thingz_sound_local_dict_table[] = {
	{ MP_OBJ_NEW_QSTR(MP_QSTR___del__),     (mp_obj_t)&mp_thingz_sound_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_play),     (mp_obj_t)&mp_thingz_sound_play_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_frequency),     (mp_obj_t)&mp_thingz_sound_set_freq_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_volume),     (mp_obj_t)&mp_thingz_sound_set_volume_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),     (mp_obj_t)&mp_thingz_sound_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_play_sample),     (mp_obj_t)&mp_thingz_sound_play_sample_obj },
};

static MP_DEFINE_CONST_DICT (
	mp_thingz_sound_local_dict,
	thingz_sound_local_dict_table
);

MP_DEFINE_CONST_OBJ_TYPE(
    thingz_sound_type,
    MP_QSTR_Sound,
    MP_TYPE_FLAG_NONE,
    make_new, mp_thingz_sound_make_new,
    locals_dict, &mp_thingz_sound_local_dict
);
