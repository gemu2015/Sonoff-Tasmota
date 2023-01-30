#include "be_constobj.h"

static be_define_const_map_slots(be_class_aes_ccm_map) {
    { be_const_key(encrypt, 4), be_const_func(m_aes_ccm_encryt) },
    { be_const_key(_X2Ep2, -1), be_const_var(0) },
    { be_const_key(decrypt, -1), be_const_func(m_aes_ccm_decryt) },
    { be_const_key(init, -1), be_const_func(m_aes_ccm_init) },
    { be_const_key(_X2Ep1, -1), be_const_var(1) },
    { be_const_key(tag, -1), be_const_func(m_aes_ccm_tag) },
};

static be_define_const_map(
    be_class_aes_ccm_map,
    6
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_aes_ccm,
    2,
    NULL,
    AES_CCM
);
