#include "be_constobj.h"

static be_define_const_map_slots(be_class_MQTT_ntv_map) {
    { be_const_key(connected, 1), be_const_ctype_func(be_mqtt_connected) },
    { be_const_key(publish, -1), be_const_func(be_mqtt_publish) },
    { be_const_key(_subscribe, -1), be_const_ctype_func(be_mqtt_subscribe) },
    { be_const_key(_unsubscribe, -1), be_const_ctype_func(be_mqtt_unsubscribe) },
};

static be_define_const_map(
    be_class_MQTT_ntv_map,
    4
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_MQTT_ntv,
    0,
    NULL,
    MQTT_ntv
);
