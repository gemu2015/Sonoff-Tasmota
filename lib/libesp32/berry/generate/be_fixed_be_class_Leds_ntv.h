#include "be_constobj.h"

static be_define_const_map_slots(be_class_Leds_ntv_map) {
    { be_const_key_weak(WS2812_GRB, 1), be_const_int(1) },
    { be_const_key_weak(_t, 3), be_const_var(1) },
    { be_const_key_weak(call_native, -1), be_const_func(be_neopixelbus_call_native) },
    { be_const_key_weak(apply_bri_gamma, -1), be_const_static_func(be_leds_apply_bri_gamma) },
    { be_const_key_weak(SK6812_GRBW, -1), be_const_int(2) },
    { be_const_key_weak(blend_color, -1), be_const_static_func(be_leds_blend_color) },
    { be_const_key_weak(_p, -1), be_const_var(0) },
};

static be_define_const_map(
    be_class_Leds_ntv_map,
    7
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_Leds_ntv,
    2,
    NULL,
    Leds_ntv
);
