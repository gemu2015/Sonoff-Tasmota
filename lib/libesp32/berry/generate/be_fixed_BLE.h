#include "be_constobj.h"

static be_define_const_map_slots(m_libBLE_map) {
    { be_const_key(set_chr, 4), be_const_ctype_func(be_BLE_set_characteristic) },
    { be_const_key(set_svc, -1), be_const_ctype_func(be_BLE_set_service) },
    { be_const_key(run, -1), be_const_ctype_func(be_BLE_run) },
    { be_const_key(init, 0), be_const_func(be_BLE_init) },
    { be_const_key(serv_cb, 5), be_const_ctype_func(be_BLE_reg_server_cb) },
    { be_const_key(loop, -1), be_const_ctype_func(be_BLE_loop) },
    { be_const_key(adv_watch, -1), be_const_ctype_func(be_BLE_adv_watch) },
    { be_const_key(info, -1), be_const_func(be_BLE_info) },
    { be_const_key(adv_block, -1), be_const_ctype_func(be_BLE_adv_block) },
    { be_const_key(conn_cb, 7), be_const_ctype_func(be_BLE_reg_conn_cb) },
    { be_const_key(adv_cb, 1), be_const_ctype_func(be_BLE_reg_adv_cb) },
    { be_const_key(set_MAC, -1), be_const_ctype_func(be_BLE_set_MAC) },
};

static be_define_const_map(
    m_libBLE_map,
    12
);

static be_define_const_module(
    m_libBLE,
    "BLE"
);

BE_EXPORT_VARIABLE be_define_const_native_module(BLE);
