#include "be_constobj.h"

static be_define_const_map_slots(be_class_rsa_map) {
    { be_const_key(rs256, -1), be_const_static_func(m_rsa_rsassa_pkcs1_v1_5) },
};

static be_define_const_map(
    be_class_rsa_map,
    1
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_rsa,
    0,
    NULL,
    RSA
);
