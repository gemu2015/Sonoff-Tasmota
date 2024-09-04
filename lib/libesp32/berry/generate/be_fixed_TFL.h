#include "be_constobj.h"

static be_define_const_map_slots(m_libTFL_map) {
    { be_const_key(begin, -1), be_const_ctype_func(be_TFL_begin) },
    { be_const_key(load, -1), be_const_ctype_func(be_TFL_load) },
    { be_const_key(stats, -1), be_const_ctype_func(be_TFL_stats) },
    { be_const_key(log, 5), be_const_ctype_func(be_TFL_log) },
    { be_const_key(output, -1), be_const_ctype_func(be_TFL_output) },
    { be_const_key(input, -1), be_const_ctype_func(be_TFL_input) },
};

static be_define_const_map(
    m_libTFL_map,
    6
);

static be_define_const_module(
    m_libTFL,
    "TFL"
);

BE_EXPORT_VARIABLE be_define_const_native_module(TFL);
