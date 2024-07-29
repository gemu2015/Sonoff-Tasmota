#include "be_constobj.h"

static be_define_const_map_slots(be_class_lv_scale_section_map) {
    { be_const_key_weak(init, -1), be_const_func(be_ntv_lv_scale_section_init) },
};

static be_define_const_map(
    be_class_lv_scale_section_map,
    1
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_lv_scale_section,
    0,
    (bclass *)&be_class_lv_obj,
    lv_scale_section
);
