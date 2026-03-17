#include "be_constobj.h"

static be_define_const_map_slots(m_libwebfiles_map) {
    { be_const_key_weak(MIME_JSON, -1), be_const_str("application/json") },
    { be_const_key_weak(MIME_BINARY, -1), be_const_str("application/octet-stream") },
    { be_const_key_weak(MIME_CSS, 6), be_const_str("text/css") },
    { be_const_key_weak(MIME_HTML, -1), be_const_str("text/html") },
    { be_const_key_weak(serve, -1), be_const_func(w_webfiles_serve) },
    { be_const_key_weak(MIME_TEXT, 2), be_const_str("text/plain") },
    { be_const_key_weak(MIME_JS, -1), be_const_str("application/javascript") },
    { be_const_key_weak(serve_file, 1), be_const_func(w_webfiles_serve_file) },
};

static be_define_const_map(
    m_libwebfiles_map,
    8
);

static be_define_const_module(
    m_libwebfiles,
    "webfiles"
);

BE_EXPORT_VARIABLE be_define_const_native_module(webfiles);
