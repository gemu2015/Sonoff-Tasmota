#include "be_constobj.h"

static be_define_const_map_slots(be_class_zb_device_map) {
    { be_const_key_weak(tostring, 3), be_const_closure(class_zb_device_tostring_closure) },
    { be_const_key_weak(member, -1), be_const_func(zd_member) },
    { be_const_key_weak(_p, 4), be_const_var(0) },
    { be_const_key_weak(init, -1), be_const_ctype_func(zd_init) },
    { be_const_key_weak(info, -1), be_const_func(zd_info) },
};

static be_define_const_map(
    be_class_zb_device_map,
    5
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_zb_device,
    1,
    NULL,
    zb_device
);
