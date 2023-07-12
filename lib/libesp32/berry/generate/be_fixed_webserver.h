#include "be_constobj.h"

static be_define_const_map_slots(m_libwebserver_map) {
    { be_const_key(state, -1), be_const_func(w_webserver_state) },
    { be_const_key(content_stop, -1), be_const_func(w_webserver_content_stop) },
    { be_const_key(on, -1), be_const_func(w_webserver_on) },
    { be_const_key(arg, 12), be_const_func(w_webserver_arg) },
    { be_const_key(check_privileged_access, -1), be_const_func(w_webserver_check_privileged_access) },
    { be_const_key(content_start, -1), be_const_func(w_webserver_content_start) },
    { be_const_key(content_open, -1), be_const_func(w_webserver_content_open) },
    { be_const_key(arg_name, -1), be_const_func(w_webserver_arg_name) },
    { be_const_key(html_escape, -1), be_const_func(w_webserver_html_escape) },
    { be_const_key(content_flush, 6), be_const_func(w_webserver_content_flush) },
    { be_const_key(has_arg, -1), be_const_func(w_webserver_has_arg) },
    { be_const_key(content_send_style, 16), be_const_func(w_webserver_content_send_style) },
    { be_const_key(arg_size, -1), be_const_func(w_webserver_argsize) },
    { be_const_key(member, 11), be_const_func(w_webserver_member) },
    { be_const_key(content_response, -1), be_const_func(w_webserver_content_response) },
    { be_const_key(redirect, -1), be_const_func(w_webserver_redirect) },
    { be_const_key(content_send, -1), be_const_func(w_webserver_content_send) },
    { be_const_key(content_button, -1), be_const_func(w_webserver_content_button) },
};

static be_define_const_map(
    m_libwebserver_map,
    18
);

static be_define_const_module(
    m_libwebserver,
    "webserver"
);

BE_EXPORT_VARIABLE be_define_const_native_module(webserver);
