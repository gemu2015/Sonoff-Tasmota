#include "be_constobj.h"

static be_define_const_map_slots(be_class_hkdf_sha256_map) {
    { be_const_key(derive, -1), be_const_static_func(m_hkdf_sha256_derive) },
};

static be_define_const_map(
    be_class_hkdf_sha256_map,
    1
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_hkdf_sha256,
    0,
    NULL,
    HKDF_SHA256
);
