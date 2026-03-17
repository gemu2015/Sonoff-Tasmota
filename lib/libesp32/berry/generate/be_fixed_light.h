#include "be_constobj.h"

static be_define_const_map_slots(m_liblight_map) {
    { be_const_key(gamma8, 3), be_const_func(l_gamma8) },
    { be_const_key(set, -1), be_const_func(l_setlight) },
    { be_const_key(get, 4), be_const_func(l_getlight) },
    { be_const_key(reverse_gamma10, -1), be_const_func(l_rev_gamma10) },
    { be_const_key(gamma10, -1), be_const_func(l_gamma10) },
    { be_const_key(set_bri, 2), be_const_ctype_func(l_set_bri) },
};

static be_define_const_map(
    m_liblight_map,
    6
);

static be_define_const_module(
    m_liblight,
    "light"
);

BE_EXPORT_VARIABLE be_define_const_native_module(light);
