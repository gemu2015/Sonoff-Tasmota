#include "be_constobj.h"

static be_define_const_map_slots(be_class_chacha_poly_map) {
    { be_const_key(chacha_run, -1), be_const_static_func(m_chacha20_run) },
    { be_const_key(poly_run, 0), be_const_static_func(m_poly1305_run) },
};

static be_define_const_map(
    be_class_chacha_poly_map,
    2
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_chacha_poly,
    0,
    NULL,
    CHACHA20_POLY1305
);
