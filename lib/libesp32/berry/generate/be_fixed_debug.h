#include "be_constobj.h"

static be_define_const_map_slots(m_libdebug_map) {
    { be_const_key(allocs, 3), be_const_func(m_allocs) },
    { be_const_key(traceback, -1), be_const_func(m_traceback) },
    { be_const_key(counters, 0), be_const_func(m_counters) },
    { be_const_key(attrdump, -1), be_const_func(m_attrdump) },
    { be_const_key(top, -1), be_const_func(m_top) },
    { be_const_key(codedump, 6), be_const_func(m_codedump) },
    { be_const_key(reallocs, -1), be_const_func(m_reallocs) },
    { be_const_key(frees, -1), be_const_func(m_frees) },
    { be_const_key(calldepth, -1), be_const_func(m_calldepth) },
};

static be_define_const_map(
    m_libdebug_map,
    9
);

static be_define_const_module(
    m_libdebug,
    "debug"
);

BE_EXPORT_VARIABLE be_define_const_native_module(debug);
