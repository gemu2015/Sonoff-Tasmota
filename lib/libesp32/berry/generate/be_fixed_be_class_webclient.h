#include "be_constobj.h"

static be_define_const_map_slots(be_class_webclient_map) {
    { be_const_key(set_timeouts, -1), be_const_func(wc_set_timeouts) },
    { be_const_key(write_flash, -1), be_const_func(wc_writeflash) },
    { be_const_key(PUT, -1), be_const_func(wc_PUT) },
    { be_const_key(write_file, 16), be_const_func(wc_writefile) },
    { be_const_key(set_useragent, -1), be_const_func(wc_set_useragent) },
    { be_const_key(PATCH, 18), be_const_func(wc_PATCH) },
    { be_const_key(begin, 8), be_const_func(wc_begin) },
    { be_const_key(add_header, -1), be_const_func(wc_addheader) },
    { be_const_key(set_auth, -1), be_const_func(wc_set_auth) },
    { be_const_key(get_string, -1), be_const_func(wc_getstring) },
    { be_const_key(DELETE, 6), be_const_func(wc_DELETE) },
    { be_const_key(close, 17), be_const_func(wc_close) },
    { be_const_key(deinit, 7), be_const_func(wc_deinit) },
    { be_const_key(get_size, -1), be_const_func(wc_getsize) },
    { be_const_key(_X2Ew, -1), be_const_var(0) },
    { be_const_key(init, -1), be_const_func(wc_init) },
    { be_const_key(GET, -1), be_const_func(wc_GET) },
    { be_const_key(POST, -1), be_const_func(wc_POST) },
    { be_const_key(url_encode, -1), be_const_func(wc_urlencode) },
    { be_const_key(_X2Ep, 3), be_const_var(1) },
};

static be_define_const_map(
    be_class_webclient_map,
    20
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_webclient,
    2,
    NULL,
    webclient
);
