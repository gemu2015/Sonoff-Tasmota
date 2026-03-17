#include "be_constobj.h"

static be_define_const_map_slots(m_libMI32_map) {
    { be_const_key(get_name, -1), be_const_ctype_func(be_MI32_get_name) },
    { be_const_key(set_hum, -1), be_const_ctype_func(be_MI32_set_hum) },
    { be_const_key(widget, 0), be_const_ctype_func(be_MI32_widget) },
    { be_const_key(get_MAC, -1), be_const_ctype_func(be_MI32_get_MAC) },
    { be_const_key(devices, 2), be_const_ctype_func(be_MI32_devices) },
    { be_const_key(set_bat, -1), be_const_ctype_func(be_MI32_set_bat) },
    { be_const_key(set_temp, -1), be_const_ctype_func(be_MI32_set_temp) },
};

static be_define_const_map(
    m_libMI32_map,
    7
);

static be_define_const_module(
    m_libMI32,
    "MI32"
);

BE_EXPORT_VARIABLE be_define_const_native_module(MI32);
