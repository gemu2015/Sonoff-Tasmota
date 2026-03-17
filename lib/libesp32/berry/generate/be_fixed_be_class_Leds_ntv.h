#include "be_constobj.h"

static be_define_const_map_slots(be_class_Leds_ntv_map) {
    { be_const_key_weak(SK6812_GRBW, 2), be_const_int(sk6812_grbw) },
    { be_const_key_weak(RMT, 3), be_const_int(TasmotaLed_RMT) },
    { be_const_key_weak(SK6812_GRB, 7), be_const_int(sk6812_grb) },
    { be_const_key_weak(_p, -1), be_const_var(0) },
    { be_const_key_weak(apply_bri_gamma, -1), be_const_static_func(be_leds_apply_bri_gamma) },
    { be_const_key_weak(SPI, -1), be_const_int(TasmotaLed_SPI) },
    { be_const_key_weak(WS2812_GRB, -1), be_const_int(ws2812_grb) },
    { be_const_key_weak(blend_color, -1), be_const_static_func(be_leds_blend_color) },
    { be_const_key_weak(call_native, -1), be_const_func(be_tasmotaled_call_native) },
    { be_const_key_weak(I2S, 5), be_const_int(TasmotaLed_I2S) },
    { be_const_key_weak(set_pixels, 6), be_const_static_func(be_leds_set_pixels) },
};

static be_define_const_map(
    be_class_Leds_ntv_map,
    11
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_Leds_ntv,
    1,
    NULL,
    Leds_ntv
);
