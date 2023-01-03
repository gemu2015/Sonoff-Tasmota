#include "be_constobj.h"

static be_define_const_map_slots(be_class_sha256_map) {
    { be_const_key(update, -1), be_const_func(m_hash_sha256_update) },
    { be_const_key(init, 2), be_const_func(m_hash_sha256_init) },
    { be_const_key(_X2Ep, -1), be_const_var(0) },
    { be_const_key(out, 1), be_const_func(m_hash_sha256_out) },
};

static be_define_const_map(
    be_class_sha256_map,
    4
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_sha256,
    1,
    NULL,
    SHA256
);
