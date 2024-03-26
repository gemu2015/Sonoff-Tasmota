#include "be_constobj.h"

static be_define_const_map_slots(be_class_aes_ctr_map) {
    { be_const_key(encrypt, -1), be_const_func(m_aes_ctr_run) },
    { be_const_key(_X2Ep1, 0), be_const_var(0) },
    { be_const_key(decrypt, 1), be_const_func(m_aes_ctr_run) },
    { be_const_key(init, -1), be_const_func(m_aes_ctr_init) },
};

static be_define_const_map(
    be_class_aes_ctr_map,
    4
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_aes_ctr,
    1,
    NULL,
    AES_CTR
);
