#include "be_constobj.h"

static be_define_const_map_slots(m_libcrypto_map) {
    { be_const_key(random, -1), be_const_func(m_crypto_random) },
    { be_const_key(member, -1), be_const_func(be_class_crypto_member) },
};

static be_define_const_map(
    m_libcrypto_map,
    2
);

static be_define_const_module(
    m_libcrypto,
    "crypto"
);

BE_EXPORT_VARIABLE be_define_const_native_module(crypto);
