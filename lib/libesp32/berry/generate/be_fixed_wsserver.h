#include "be_constobj.h"

static be_define_const_map_slots(m_libwsserver_map) {
    { be_const_key_weak(send, 12), be_const_func(w_wsserver_send) },
    { be_const_key_weak(BINARY, 11), be_const_int(HTTPD_WS_TYPE_BINARY) },
    { be_const_key_weak(on, 1), be_const_func(w_wsserver_on) },
    { be_const_key_weak(stop, -1), be_const_func(w_wsserver_stop) },
    { be_const_key_weak(MAX_CLIENTS, -1), be_const_int(MAX_WS_CLIENTS) },
    { be_const_key_weak(CONNECT, 6), be_const_int(WSSERVER_EVENT_CONNECT) },
    { be_const_key_weak(is_connected, -1), be_const_func(w_wsserver_is_connected) },
    { be_const_key_weak(DISCONNECT, -1), be_const_int(WSSERVER_EVENT_DISCONNECT) },
    { be_const_key_weak(TEXT, -1), be_const_int(HTTPD_WS_TYPE_TEXT) },
    { be_const_key_weak(close, 0), be_const_func(w_wsserver_close) },
    { be_const_key_weak(count_clients, -1), be_const_func(w_wsserver_count_clients) },
    { be_const_key_weak(MESSAGE, -1), be_const_int(WSSERVER_EVENT_MESSAGE) },
    { be_const_key_weak(client_info, -1), be_const_func(w_wsserver_client_info) },
    { be_const_key_weak(start, 10), be_const_func(w_wsserver_start) },
};

static be_define_const_map(
    m_libwsserver_map,
    14
);

static be_define_const_module(
    m_libwsserver,
    "wsserver"
);

BE_EXPORT_VARIABLE be_define_const_native_module(wsserver);
