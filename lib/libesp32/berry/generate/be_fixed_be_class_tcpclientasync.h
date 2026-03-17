#include "be_constobj.h"

static be_define_const_map_slots(be_class_tcpclientasync_map) {
    { be_const_key(close, -1), be_const_func(wc_tcpasync_close) },
    { be_const_key(connect, -1), be_const_func(wc_tcpasync_connect) },
    { be_const_key(deinit, 11), be_const_func(wc_tcpasync_deinit) },
    { be_const_key(init, 0), be_const_func(wc_tcpasync_init) },
    { be_const_key(connected, 2), be_const_func(wc_tcpasync_connected) },
    { be_const_key(write, -1), be_const_func(wc_tcpasync_write) },
    { be_const_key(listening, -1), be_const_func(wc_tcpasync_listening) },
    { be_const_key(_X2Ep, -1), be_const_var(0) },
    { be_const_key(available, 5), be_const_func(wc_tcpasync_available) },
    { be_const_key(info, 10), be_const_func(wc_tcpasync_info) },
    { be_const_key(read, -1), be_const_func(wc_tcpasync_read) },
    { be_const_key(readbytes, -1), be_const_func(wc_tcpasync_readbytes) },
};

static be_define_const_map(
    be_class_tcpclientasync_map,
    12
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_tcpclientasync,
    1,
    NULL,
    tcpclientasync
);
