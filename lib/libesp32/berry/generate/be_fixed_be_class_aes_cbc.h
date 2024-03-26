#include "be_constobj.h"

static be_define_const_map_slots(be_class_aes_cbc_map) {
    { be_const_key(decrypt1, -1), be_const_static_func(m_aes_cbc_decrypt1) },
    { be_const_key(encrypt1, 0), be_const_static_func(m_aes_cbc_encrypt1) },
};

static be_define_const_map(
    be_class_aes_cbc_map,
    2
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_aes_cbc,
    0,
    NULL,
    AES_CBC
);
