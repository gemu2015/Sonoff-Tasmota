#include "be_constobj.h"

static be_define_const_map_slots(m_libhttpserver_map) {
    { be_const_key_weak(start, -1), be_const_func(w_httpserver_start) },
    { be_const_key_weak(send, 0), be_const_func(w_httpserver_send) },
    { be_const_key_weak(stop, -1), be_const_func(w_httpserver_stop) },
    { be_const_key_weak(process_queue, -1), be_const_func(w_httpserver_process_queue) },
    { be_const_key_weak(on, 3), be_const_func(w_httpserver_on) },
};

static be_define_const_map(
    m_libhttpserver_map,
    5
);

static be_define_const_module(
    m_libhttpserver,
    "httpserver"
);

BE_EXPORT_VARIABLE be_define_const_native_module(httpserver);
