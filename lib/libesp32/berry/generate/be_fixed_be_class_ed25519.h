#include "be_constobj.h"

static be_define_const_map_slots(be_class_ed25519_map) {
    { be_const_key(secret_key, -1), be_const_func(m_ed25519_secret_key) },
    { be_const_key(verify, -1), be_const_func(m_ed25519_verify) },
    { be_const_key(sign, -1), be_const_func(m_ed25519_sign) },
};

static be_define_const_map(
    be_class_ed25519_map,
    3
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_ed25519,
    0,
    NULL,
    ED25519
);
