#include "be_constobj.h"

static be_define_const_map_slots(be_class_webclient_map) {
    { be_const_key(write_file, 15), be_const_func(wc_writefile) },
    { be_const_key(PATCH, -1), be_const_func(wc_PATCH) },
    { be_const_key(_X2Ew, -1), be_const_var(0) },
    { be_const_key(get_bytes, 12), be_const_func(wc_getbytes) },
    { be_const_key(add_header, 11), be_const_func(wc_addheader) },
    { be_const_key(write_flash, -1), be_const_func(wc_writeflash) },
    { be_const_key(begin, -1), be_const_func(wc_begin) },
    { be_const_key(POST, 0), be_const_func(wc_POST) },
    { be_const_key(collect_headers, -1), be_const_func(wc_collect_headers) },
    { be_const_key(url_encode, -1), be_const_static_func(wc_urlencode) },
    { be_const_key(set_auth, -1), be_const_func(wc_set_auth) },
    { be_const_key(set_useragent, 20), be_const_func(wc_set_useragent) },
    { be_const_key(init, 22), be_const_func(wc_init) },
    { be_const_key(get_header, -1), be_const_func(wc_get_header) },
    { be_const_key(PUT, -1), be_const_func(wc_PUT) },
    { be_const_key(set_follow_redirects, -1), be_const_func(wc_set_follow_redirects) },
    { be_const_key(deinit, -1), be_const_func(wc_deinit) },
    { be_const_key(get_string, 21), be_const_func(wc_getstring) },
    { be_const_key(DELETE, 10), be_const_func(wc_DELETE) },
    { be_const_key(_X2Ep, -1), be_const_var(1) },
    { be_const_key(set_timeouts, -1), be_const_func(wc_set_timeouts) },
    { be_const_key(get_size, -1), be_const_func(wc_getsize) },
    { be_const_key(close, -1), be_const_func(wc_close) },
    { be_const_key(GET, -1), be_const_func(wc_GET) },
};

static be_define_const_map(
    be_class_webclient_map,
    24
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_webclient,
    2,
    NULL,
    webclient
);
