#include "be_constobj.h"

static be_define_const_map_slots(be_class_webclient_map) {
    { be_const_key(write_file, -1), be_const_func(wc_writefile) },
    { be_const_key(GET, -1), be_const_func(wc_GET) },
    { be_const_key(_X2Ew, -1), be_const_var(0) },
    { be_const_key(get_header, 8), be_const_func(wc_get_header) },
    { be_const_key(set_useragent, 18), be_const_func(wc_set_useragent) },
    { be_const_key(get_size, 9), be_const_func(wc_getsize) },
    { be_const_key(_X2Ep, -1), be_const_var(1) },
    { be_const_key(PATCH, -1), be_const_func(wc_PATCH) },
    { be_const_key(set_follow_redirects, 2), be_const_func(wc_set_follow_redirects) },
    { be_const_key(set_timeouts, 20), be_const_func(wc_set_timeouts) },
    { be_const_key(collect_headers, 22), be_const_func(wc_collect_headers) },
    { be_const_key(set_auth, -1), be_const_func(wc_set_auth) },
    { be_const_key(url_encode, 7), be_const_func(wc_urlencode) },
    { be_const_key(get_string, -1), be_const_func(wc_getstring) },
    { be_const_key(add_header, 4), be_const_func(wc_addheader) },
    { be_const_key(DELETE, -1), be_const_func(wc_DELETE) },
    { be_const_key(POST, 0), be_const_func(wc_POST) },
    { be_const_key(PUT, 3), be_const_func(wc_PUT) },
    { be_const_key(init, -1), be_const_func(wc_init) },
    { be_const_key(deinit, -1), be_const_func(wc_deinit) },
    { be_const_key(write_flash, -1), be_const_func(wc_writeflash) },
    { be_const_key(begin, -1), be_const_func(wc_begin) },
    { be_const_key(close, -1), be_const_func(wc_close) },
};

static be_define_const_map(
    be_class_webclient_map,
    23
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_webclient,
    2,
    NULL,
    webclient
);
