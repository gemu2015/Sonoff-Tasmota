#include "be_constobj.h"

static be_define_const_map_slots(be_class_AudioInputI2S_map) {
    { be_const_key_weak(read_bytes, -1), be_const_func(be_audio_input_i2s_read_bytes) },
    { be_const_key_weak(init, -1), be_const_ctype_func(be_audio_input_i2s_init) },
    { be_const_key_weak(peak, -1), be_const_var(0) },
    { be_const_key_weak(set_gain, -1), be_const_ctype_func(be_audio_input_set_gain) },
    { be_const_key_weak(rms_bytes, -1), be_const_static_ctype_func(be_audio_input_i2s_rms_bytes) },
    { be_const_key_weak(_X2Ep, 9), be_const_var(1) },
    { be_const_key_weak(begin, -1), be_const_ctype_func(be_audio_input_i2s_begin) },
    { be_const_key_weak(get_bits_per_sample, -1), be_const_ctype_func(be_audio_input_i2s_get_bits_per_sample) },
    { be_const_key_weak(set_lowpass_alpha, 7), be_const_ctype_func(be_audio_input_i2s_set_lowpass_alpha) },
    { be_const_key_weak(get_rate, 2), be_const_ctype_func(be_audio_input_i2s_get_rate) },
    { be_const_key_weak(get_channels, -1), be_const_ctype_func(be_audio_input_i2s_get_channels) },
    { be_const_key_weak(deinit, 16), be_const_ctype_func(be_audio_input_i2s_deinit) },
    { be_const_key_weak(get_gain, 6), be_const_ctype_func(be_audio_input_i2s_get_gain) },
    { be_const_key_weak(stop, 5), be_const_ctype_func(be_audio_input_i2s_stop) },
    { be_const_key_weak(get_lowpass_alpha, 10), be_const_ctype_func(be_audio_input_i2s_get_lowpass_alpha) },
    { be_const_key_weak(sqrt_fast, 1), be_const_static_ctype_func(be_audio_input_i2s_sqrt_fast) },
    { be_const_key_weak(get_dc_offset, -1), be_const_ctype_func(be_audio_input_i2s_get_dc_offset) },
};

static be_define_const_map(
    be_class_AudioInputI2S_map,
    17
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_AudioInputI2S,
    2,
    NULL,
    AudioInputI2S
);
