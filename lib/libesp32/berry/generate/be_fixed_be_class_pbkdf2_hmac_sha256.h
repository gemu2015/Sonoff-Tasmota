#include "be_constobj.h"

static be_define_const_map_slots(be_class_pbkdf2_hmac_sha256_map) {
    { be_const_key(_f, 1), be_const_static_func(m_pbkdf2_hmac_sha256_f) },
    { be_const_key(derive, -1), be_const_closure(PBKDF2_HMAC_SHA256_closure) },
};

static be_define_const_map(
    be_class_pbkdf2_hmac_sha256_map,
    2
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_pbkdf2_hmac_sha256,
    0,
    NULL,
    PBKDF2_HMAC_SHA256
);
