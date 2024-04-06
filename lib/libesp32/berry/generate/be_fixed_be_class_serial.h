#include "be_constobj.h"

static be_define_const_map_slots(be_class_serial_map) {
    { be_const_key(SERIAL_5N1, -1), be_const_int(SERIAL_5N1) },
    { be_const_key(SERIAL_5E2, 15), be_const_int(SERIAL_5E2) },
    { be_const_key(SERIAL_7N2, 30), be_const_int(SERIAL_7N2) },
    { be_const_key(SERIAL_5E1, 2), be_const_int(SERIAL_5E1) },
    { be_const_key(SERIAL_5O2, -1), be_const_int(SERIAL_5O2) },
    { be_const_key(read, -1), be_const_func(b_serial_read) },
    { be_const_key(SERIAL_7O2, 11), be_const_int(SERIAL_7O2) },
    { be_const_key(SERIAL_5O1, -1), be_const_int(SERIAL_5O1) },
    { be_const_key(deinit, -1), be_const_func(b_serial_deinit) },
    { be_const_key(SERIAL_8E2, -1), be_const_int(SERIAL_8E2) },
    { be_const_key(SERIAL_6N2, -1), be_const_int(SERIAL_6N2) },
    { be_const_key(SERIAL_8N2, -1), be_const_int(SERIAL_8N2) },
    { be_const_key(SERIAL_7O1, 23), be_const_int(SERIAL_7O1) },
    { be_const_key(SERIAL_7E1, -1), be_const_int(SERIAL_7E1) },
    { be_const_key(SERIAL_6E1, -1), be_const_int(SERIAL_6E1) },
    { be_const_key(SERIAL_7N1, -1), be_const_int(SERIAL_7N1) },
    { be_const_key(SERIAL_8E1, -1), be_const_int(SERIAL_8E1) },
    { be_const_key(SERIAL_6O2, -1), be_const_int(SERIAL_6O2) },
    { be_const_key(SERIAL_6O1, -1), be_const_int(SERIAL_6O1) },
    { be_const_key(init, 12), be_const_func(b_serial_init) },
    { be_const_key(SERIAL_7E2, -1), be_const_int(SERIAL_7E2) },
    { be_const_key(SERIAL_6N1, -1), be_const_int(SERIAL_6N1) },
    { be_const_key(SERIAL_8O1, 1), be_const_int(SERIAL_8O1) },
    { be_const_key(SERIAL_8N1, 26), be_const_int(SERIAL_8N1) },
    { be_const_key(available, 18), be_const_func(b_serial_available) },
    { be_const_key(SERIAL_5N2, -1), be_const_int(SERIAL_5N2) },
    { be_const_key(_X2Ep, -1), be_const_var(0) },
    { be_const_key(SERIAL_6E2, -1), be_const_int(SERIAL_6E2) },
    { be_const_key(write, 10), be_const_func(b_serial_write) },
    { be_const_key(flush, 7), be_const_func(b_serial_flush) },
    { be_const_key(SERIAL_8O2, 31), be_const_int(SERIAL_8O2) },
    { be_const_key(close, -1), be_const_func(b_serial_deinit) },
};

static be_define_const_map(
    be_class_serial_map,
    32
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_serial,
    1,
    NULL,
    serial
);
