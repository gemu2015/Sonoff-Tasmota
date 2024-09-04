#include "be_constobj.h"

static be_define_const_map_slots(be_class_zb_coord_ntv_map) {
    { be_const_key_weak(zcl_attribute, -1), be_const_class(be_class_zcl_attribute) },
    { be_const_key_weak(zcl_attribute_list_ntv, 2), be_const_class(be_class_zcl_attribute_list_ntv) },
    { be_const_key_weak(started, 6), be_const_func(zc_started) },
    { be_const_key_weak(test_attr, 5), be_const_func(zigbee_test_attr) },
    { be_const_key_weak(zcl_frame, 11), be_const_class(be_class_zcl_frame) },
    { be_const_key_weak(size, -1), be_const_ctype_func(zc_size) },
    { be_const_key_weak(test_msg, 10), be_const_func(zigbee_test_msg) },
    { be_const_key_weak(find, 0), be_const_func(zc_find) },
    { be_const_key_weak(info, -1), be_const_func(zc_info) },
    { be_const_key_weak(zcl_attribute_list, 3), be_const_class(be_class_zcl_attribute_list) },
    { be_const_key_weak(item, -1), be_const_func(zc_item) },
    { be_const_key_weak(iter, -1), be_const_func(zc_iter) },
    { be_const_key_weak(zcl_attribute_ntv, 8), be_const_class(be_class_zcl_attribute_ntv) },
    { be_const_key_weak(zb_device, -1), be_const_class(be_class_zb_device) },
    { be_const_key_weak(abort, -1), be_const_ctype_func(zc_abort) },
};

static be_define_const_map(
    be_class_zb_coord_ntv_map,
    15
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_zb_coord_ntv,
    0,
    NULL,
    zb_coord_ntv
);
