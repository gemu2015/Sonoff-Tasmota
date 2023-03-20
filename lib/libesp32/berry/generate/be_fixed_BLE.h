#include "be_constobj.h"

static be_define_const_map_slots(m_libBLE_map) {
    { be_const_key(adv_block, -1), be_const_ctype_func(be_BLE_adv_block) },
    { be_const_key(conn_cb, -1), be_const_ctype_func(be_BLE_reg_conn_cb) },
    { be_const_key(adv_cb, 5), be_const_ctype_func(be_BLE_reg_adv_cb) },
    { be_const_key(set_MAC, -1), be_const_ctype_func(be_BLE_set_MAC) },
    { be_const_key(adv_watch, -1), be_const_ctype_func(be_BLE_adv_watch) },
    { be_const_key(run, -1), be_const_ctype_func(be_BLE_run) },
    { be_const_key(set_svc, 4), be_const_ctype_func(be_BLE_set_service) },
    { be_const_key(set_chr, 3), be_const_ctype_func(be_BLE_set_characteristic) },
};

static be_define_const_map(
    m_libBLE_map,
    8
);

static be_define_const_module(
    m_libBLE,
    "BLE"
);

BE_EXPORT_VARIABLE be_define_const_native_module(BLE);
