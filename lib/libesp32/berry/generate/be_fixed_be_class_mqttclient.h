#include "be_constobj.h"

static be_define_const_map_slots(be_class_mqttclient_map) {
    { be_const_key(_X2Eon_message, 2), be_const_var(1) },
    { be_const_key(_X2Ep, 15), be_const_var(0) },
    { be_const_key(connected, -1), be_const_func(be_mqttc_connected) },
    { be_const_key(subscribe, 1), be_const_func(be_mqttc_subscribe) },
    { be_const_key(set_on_connect, -1), be_const_func(be_mqttc_set_on_connect) },
    { be_const_key(set_on_message, -1), be_const_func(be_mqttc_set_on_message) },
    { be_const_key(unsubscribe, 14), be_const_func(be_mqttc_unsubscribe) },
    { be_const_key(connect, -1), be_const_func(be_mqttc_connect) },
    { be_const_key(deinit, 12), be_const_func(be_mqttc_deinit) },
    { be_const_key(set_auto_reconnect, 7), be_const_func(be_mqttc_set_auto_reconnect) },
    { be_const_key(loop, -1), be_const_func(be_mqttc_loop) },
    { be_const_key(disconnect, 10), be_const_func(be_mqttc_disconnect) },
    { be_const_key(publish, -1), be_const_func(be_mqttc_publish) },
    { be_const_key(_X2Eon_connect, -1), be_const_var(2) },
    { be_const_key(state, -1), be_const_func(be_mqttc_state) },
    { be_const_key(init, -1), be_const_func(be_mqttc_init) },
};

static be_define_const_map(
    be_class_mqttclient_map,
    16
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_mqttclient,
    3,
    NULL,
    mqttclient
);
