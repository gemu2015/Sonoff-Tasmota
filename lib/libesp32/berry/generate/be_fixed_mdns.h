#include "be_constobj.h"

static be_define_const_map_slots(m_libmdns_map) {
    { be_const_key(add_service, -1), be_const_func(m_mdns_add_service) },
    { be_const_key(set_hostname, 2), be_const_ctype_func(m_mdns_set_hostname) },
    { be_const_key(stop, -1), be_const_ctype_func(m_mdns_stop) },
    { be_const_key(start, -1), be_const_ctype_func(m_mdns_start) },
};

static be_define_const_map(
    m_libmdns_map,
    4
);

static be_define_const_module(
    m_libmdns,
    "mdns"
);

BE_EXPORT_VARIABLE be_define_const_native_module(mdns);
