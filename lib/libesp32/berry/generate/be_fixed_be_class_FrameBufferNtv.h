#include "be_constobj.h"

static be_define_const_map_slots(be_class_FrameBufferNtv_map) {
    { be_const_key_weak(blend, 2), be_const_static_func(be_animation_ntv_blend) },
    { be_const_key_weak(apply_opacity, -1), be_const_static_func(be_animation_ntv_apply_opacity) },
    { be_const_key_weak(blend_color, 5), be_const_static_func(be_animation_ntv_blend_color) },
    { be_const_key_weak(gradient_fill, 7), be_const_static_func(be_animation_ntv_gradient_fill) },
    { be_const_key_weak(blend_pixels, 6), be_const_static_func(be_animation_ntv_blend_pixels) },
    { be_const_key_weak(blend_linear, -1), be_const_static_func(be_animation_ntv_blend_linear) },
    { be_const_key_weak(fill_pixels, -1), be_const_static_func(be_animation_ntv_fill_pixels) },
    { be_const_key_weak(apply_brightness, -1), be_const_static_func(be_animation_ntv_apply_brightness) },
};

static be_define_const_map(
    be_class_FrameBufferNtv_map,
    8
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_FrameBufferNtv,
    0,
    NULL,
    FrameBufferNtv
);
