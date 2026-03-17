#include "be_constobj.h"

static be_define_const_map_slots(be_class_img_map) {
    { be_const_key_weak(from_jpg, -1), be_const_func(be_img_from_jpg) },
    { be_const_key_weak(_X2Ep, 10), be_const_var(0) },
    { be_const_key_weak(from_buffer, -1), be_const_func(be_img_from_buffer) },
    { be_const_key_weak(RGB888, -1), be_const_int(5) },
    { be_const_key_weak(get_buffer, 3), be_const_func(be_img_get_buffer) },
    { be_const_key_weak(RGB565LE, -1), be_const_int(1) },
    { be_const_key_weak(init, -1), be_const_func(be_img_init) },
    { be_const_key_weak(deinit, 8), be_const_func(be_img_deinit) },
    { be_const_key_weak(RGB565, 9), be_const_int(0) },
    { be_const_key_weak(info, -1), be_const_func(be_img_info) },
    { be_const_key_weak(JPEG, -1), be_const_int(4) },
    { be_const_key_weak(GRAYSCALE, 1), be_const_int(3) },
    { be_const_key_weak(convert_to, 7), be_const_func(be_img_convert_to) },
};

static be_define_const_map(
    be_class_img_map,
    13
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_img,
    1,
    NULL,
    img
);
