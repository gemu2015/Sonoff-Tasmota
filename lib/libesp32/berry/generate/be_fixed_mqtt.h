#include "be_constobj.h"

static be_define_const_map_slots(m_libmqtt_map) {
    { be_const_key(init, -1), be_const_func(zigbee_init) },
};

static be_define_const_map(
    m_libmqtt_map,
    1
);

static be_define_const_module(
    m_libmqtt,
    "mqtt"
);

BE_EXPORT_VARIABLE be_define_const_native_module(mqtt);
