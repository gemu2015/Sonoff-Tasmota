#include "be_constobj.h"

static be_define_const_map_slots(be_class_pixmat_map) {
    { be_const_key_weak(_buf, -1), be_const_var(1) },
    { be_const_key_weak(_X2Ep, -1), be_const_var(0) },
    { be_const_key_weak(get, -1), be_const_func(be_pixmat_get) },
    { be_const_key_weak(scroll, 0), be_const_func(be_pixmat_scroll) },
    { be_const_key_weak(deinit, -1), be_const_func(be_pixmat_deinit) },
    { be_const_key_weak(clear, 2), be_const_func(be_pixmat_clear) },
    { be_const_key_weak(init, 8), be_const_func(be_pixmat_init) },
    { be_const_key_weak(set, -1), be_const_func(be_pixmat_set) },
    { be_const_key_weak(blit, -1), be_const_func(be_pixmat_blit) },
};

static be_define_const_map(
    be_class_pixmat_map,
    9
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_pixmat,
    2,
    NULL,
    pixmat
);
