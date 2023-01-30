#include "be_constobj.h"

static be_define_const_map_slots(be_class_re_pattern_map) {
    { be_const_key(_p, -1), be_const_var(0) },
    { be_const_key(search, -1), be_const_func(re_pattern_search) },
    { be_const_key(match, 0), be_const_func(re_pattern_match) },
    { be_const_key(split, -1), be_const_func(re_pattern_split) },
};

static be_define_const_map(
    be_class_re_pattern_map,
    4
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_re_pattern,
    1,
    NULL,
    re_pattern
);
