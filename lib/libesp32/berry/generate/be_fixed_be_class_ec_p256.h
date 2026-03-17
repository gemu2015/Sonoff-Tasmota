#include "be_constobj.h"

static be_define_const_map_slots(be_class_ec_p256_map) {
    { be_const_key(public_key, 8), be_const_static_func(m_ec_p256_pubkey) },
    { be_const_key(mod, -1), be_const_static_func(m_ec_p256_mod) },
    { be_const_key(ecdsa_sign_sha256_asn1, 6), be_const_static_func(m_ec_p256_ecdsa_sign_sha256_asn1) },
    { be_const_key(mul, 2), be_const_static_func(m_ec_p256_mul) },
    { be_const_key(shared_key, -1), be_const_static_func(m_ec_p256_sharedkey) },
    { be_const_key(ecdsa_verify_sha256, -1), be_const_static_func(m_ec_p256_ecdsa_verify_sha256) },
    { be_const_key(ecdsa_sign_sha256, -1), be_const_static_func(m_ec_p256_ecdsa_sign_sha256) },
    { be_const_key(ecdsa_verify_sha256_asn1, -1), be_const_static_func(m_ec_p256_ecdsa_verify_sha256_asn1) },
    { be_const_key(muladd, -1), be_const_static_func(m_ec_p256_muladd) },
    { be_const_key(neg, -1), be_const_static_func(m_ec_p256_neg) },
};

static be_define_const_map(
    be_class_ec_p256_map,
    10
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_ec_p256,
    0,
    NULL,
    EC_P256
);
