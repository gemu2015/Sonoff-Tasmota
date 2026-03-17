#include "be_constobj.h"

static be_define_const_map_slots(m_libcam_map) {
    { be_const_key(info, -1), be_const_func(be_cam_info) },
    { be_const_key(get_image, 0), be_const_func(be_cam_get_image) },
    { be_const_key(setup, -1), be_const_func(be_cam_setup) },
    { be_const_key(init, -1), be_const_func(be_cam_init) },
};

static be_define_const_map(
    m_libcam_map,
    4
);

static be_define_const_module(
    m_libcam,
    "cam"
);

BE_EXPORT_VARIABLE be_define_const_native_module(cam);
