#include "be_constobj.h"

static be_define_const_map_slots(be_class_tcpserver_map) {
    { be_const_key(acceptasync, -1), be_const_ctype_func(tcpserver_acceptasync) },
    { be_const_key(init, -1), be_const_ctype_func(tcpserver_init) },
    { be_const_key(accept, 4), be_const_ctype_func(tcpserver_accept) },
    { be_const_key(hasclient, 0), be_const_ctype_func(tcpserver_hasclient) },
    { be_const_key(close, -1), be_const_ctype_func(tcpserver_close) },
    { be_const_key(deinit, 1), be_const_ctype_func(tcpserver_deinit) },
    { be_const_key(_p, -1), be_const_var(0) },
};

static be_define_const_map(
    be_class_tcpserver_map,
    7
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_tcpserver,
    1,
    NULL,
    tcpserver
);
