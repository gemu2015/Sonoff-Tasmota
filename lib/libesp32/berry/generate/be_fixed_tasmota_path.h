#include "be_constobj.h"

static be_define_const_map_slots(m_libpath_map) {
    { be_const_key(listdir, 3), be_const_func(m_path_listdir) },
    { be_const_key(last_modified, -1), be_const_func(m_path_last_modified) },
    { be_const_key(format, -1), be_const_func(m_path_format) },
    { be_const_key(mkdir, -1), be_const_func(m_path_mkdir) },
    { be_const_key(isdir, -1), be_const_func(m_path_isdir) },
    { be_const_key(exists, 6), be_const_func(m_path_exists) },
    { be_const_key(remove, -1), be_const_func(m_path_remove) },
    { be_const_key(rmdir, -1), be_const_func(m_path_rmdir) },
};

static be_define_const_map(
    m_libpath_map,
    8
);

static be_define_const_module(
    m_libpath,
    "path"
);

BE_EXPORT_VARIABLE be_define_const_native_module(path);
