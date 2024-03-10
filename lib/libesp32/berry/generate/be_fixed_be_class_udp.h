#include "be_constobj.h"

static be_define_const_map_slots(be_class_udp_map) {
    { be_const_key(read, -1), be_const_func(be_udp_read) },
    { be_const_key(send_multicast, 3), be_const_ctype_func(be_udp_send_mcast_ntv) },
    { be_const_key(deinit, -1), be_const_ctype_func(be_udp_deinit_ntv) },
    { be_const_key(close, -1), be_const_ctype_func(be_udp_stop_ntv) },
    { be_const_key(remote_port, -1), be_const_var(0) },
    { be_const_key(remote_ip, 0), be_const_var(1) },
    { be_const_key(begin_multicast, -1), be_const_ctype_func(be_udp_begin_mcast_ntv) },
    { be_const_key(send, -1), be_const_ctype_func(be_udp_send_ntv) },
    { be_const_key(_X2Ep, -1), be_const_var(2) },
    { be_const_key(init, -1), be_const_ctype_func(be_udp_init_ntv) },
    { be_const_key(begin, -1), be_const_ctype_func(be_udp_begin_ntv) },
};

static be_define_const_map(
    be_class_udp_map,
    11
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_udp,
    3,
    NULL,
    udp
);
