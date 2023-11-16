#include "be_constobj.h"

static be_define_const_map_slots(be_class_Matter_Verhoeff_map) {
    { be_const_key_weak(checksum, -1), be_const_static_ctype_func(vh_checksum) },
    { be_const_key_weak(validate, -1), be_const_static_ctype_func(vh_validate) },
};

static be_define_const_map(
    be_class_Matter_Verhoeff_map,
    2
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_Matter_Verhoeff,
    0,
    NULL,
    Matter_Verhoeff
);
