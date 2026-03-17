#include "be_constobj.h"

static be_define_const_map_slots(be_class_webclient_map) {
    { be_const_key(init, 24), be_const_func(wc_init) },
    { be_const_key(GET, -1), be_const_func(wc_GET) },
    { be_const_key(collect_headers, 27), be_const_func(wc_collect_headers) },
    { be_const_key(async_state, 10), be_const_func(wc_async_state) },
    { be_const_key(last_code, 31), be_const_var(5) },
    { be_const_key(POST, -1), be_const_func(wc_POST) },
    { be_const_key(set_timeouts, -1), be_const_func(wc_set_timeouts) },
    { be_const_key(_X2Ep, -1), be_const_var(0) },
    { be_const_key(write_flash, -1), be_const_func(wc_writeflash) },
    { be_const_key(set_useragent, -1), be_const_func(wc_set_useragent) },
    { be_const_key(close, 34), be_const_func(wc_close) },
    { be_const_key(set_auth, 1), be_const_func(wc_set_auth) },
    { be_const_key(tls_set_rsa_only, -1), be_const_func(wc_tls_set_rsa_only) },
    { be_const_key(get_string, -1), be_const_func(wc_getstring) },
    { be_const_key(tls_pin_pubkey, -1), be_const_func(wc_tls_pin_pubkey) },
    { be_const_key(write_file, -1), be_const_func(wc_writefile) },
    { be_const_key(get_size, -1), be_const_func(wc_getsize) },
    { be_const_key(DELETE, 13), be_const_func(wc_DELETE) },
    { be_const_key(deinit, 32), be_const_func(wc_deinit) },
    { be_const_key(get_bytes, 5), be_const_func(wc_getbytes) },
    { be_const_key(tls_clear_pins, 9), be_const_func(wc_tls_clear_pins) },
    { be_const_key(PUT, -1), be_const_func(wc_PUT) },
    { be_const_key(PATCH, -1), be_const_func(wc_PATCH) },
    { be_const_key(async_post_start, -1), be_const_func(wc_async_post_start) },
    { be_const_key(_X2Ew, -1), be_const_var(1) },
    { be_const_key(_X2E__async_hold, -1), be_const_var(4) },
    { be_const_key(_X2E_last_code, -1), be_const_var(2) },
    { be_const_key(set_follow_redirects, -1), be_const_func(wc_set_follow_redirects) },
    { be_const_key(_X2E_last_rx, -1), be_const_var(3) },
    { be_const_key(url_encode, -1), be_const_static_func(wc_urlencode) },
    { be_const_key(get_header, -1), be_const_func(wc_get_header) },
    { be_const_key(add_header, -1), be_const_func(wc_addheader) },
    { be_const_key(async_get_start, -1), be_const_func(wc_async_get_start) },
    { be_const_key(last_rx, -1), be_const_var(6) },
    { be_const_key(async_abort, -1), be_const_func(wc_async_abort) },
    { be_const_key(addheader, 8), be_const_func(wc_addheader) },
    { be_const_key(begin, -1), be_const_func(wc_begin) },
};

static be_define_const_map(
    be_class_webclient_map,
    37
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_webclient,
    7,
    NULL,
    webclient
);
