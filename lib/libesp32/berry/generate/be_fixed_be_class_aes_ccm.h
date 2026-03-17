#include "be_constobj.h"

static be_define_const_map_slots(be_class_aes_ccm_map) {
    { be_const_key(encrypt, -1), be_const_func(m_aes_ccm_encrypt) },
    { be_const_key(decrypt1, 5), be_const_static_func(m_aes_ccm_decrypt1) },
    { be_const_key(decrypt, 0), be_const_func(m_aes_ccm_decrypt) },
    { be_const_key(tag, 4), be_const_func(m_aes_ccm_tag) },
    { be_const_key(init, 7), be_const_func(m_aes_ccm_init) },
    { be_const_key(encrypt1, -1), be_const_static_func(m_aes_ccm_encrypt1) },
    { be_const_key(_X2Ep1, -1), be_const_var(0) },
    { be_const_key(_X2Ep2, -1), be_const_var(1) },
};

static be_define_const_map(
    be_class_aes_ccm_map,
    8
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_aes_ccm,
    2,
    NULL,
    AES_CCM
);
