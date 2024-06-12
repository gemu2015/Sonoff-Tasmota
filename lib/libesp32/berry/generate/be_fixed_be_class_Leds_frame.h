#include "be_constobj.h"

static be_define_const_map_slots(be_class_Leds_frame_map) {
    { be_const_key_weak(paste_pixels, 3), be_const_func(be_leds_paste_pixels) },
    { be_const_key_weak(fill_pixels, 7), be_const_func(be_leds_fill_pixels) },
    { be_const_key_weak(setitem, -1), be_const_closure(class_Leds_frame_be_setitem_closure) },
    { be_const_key_weak(pixel_size, -1), be_const_var(0) },
    { be_const_key_weak(item, -1), be_const_closure(class_Leds_frame_be_item_closure) },
    { be_const_key_weak(blend_pixels, 2), be_const_func(be_leds_blend_pixels) },
    { be_const_key_weak(init, 0), be_const_closure(class_Leds_frame_be_init_closure) },
    { be_const_key_weak(blend, -1), be_const_static_func(be_leds_blend) },
    { be_const_key_weak(set_pixel, 1), be_const_closure(class_Leds_frame_be_set_pixel_closure) },
};

static be_define_const_map(
    be_class_Leds_frame_map,
    9
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_Leds_frame,
    1,
    (bclass *)&be_class_bytes,
    Leds_frame
);
