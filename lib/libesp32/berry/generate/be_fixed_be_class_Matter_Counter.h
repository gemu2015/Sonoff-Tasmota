#include "be_constobj.h"

static be_define_const_map_slots(be_class_Matter_Counter_map) {
    { be_const_key_weak(deinit, 1), be_const_ctype_func(mc_deinit) },
    { be_const_key_weak(next, 6), be_const_ctype_func(mc_next) },
    { be_const_key_weak(_p, -1), be_const_var(0) },
    { be_const_key_weak(validate, 7), be_const_ctype_func(mc_validate) },
    { be_const_key_weak(val, -1), be_const_ctype_func(mc_val) },
    { be_const_key_weak(tostring, -1), be_const_func(mc_tostring) },
    { be_const_key_weak(reset, -1), be_const_ctype_func(mc_reset) },
    { be_const_key_weak(init, -1), be_const_ctype_func(mc_init) },
};

static be_define_const_map(
    be_class_Matter_Counter_map,
    8
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_Matter_Counter,
    1,
    NULL,
    Matter_Counter
);
