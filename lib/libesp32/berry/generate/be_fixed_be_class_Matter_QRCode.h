#include "be_constobj.h"

static be_define_const_map_slots(be_class_Matter_QRCode_map) {
    { be_const_key_weak(encode_str, -1), be_const_static_func(qr_encode_str) },
};

static be_define_const_map(
    be_class_Matter_QRCode_map,
    1
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_Matter_QRCode,
    0,
    NULL,
    Matter_QRCode
);
