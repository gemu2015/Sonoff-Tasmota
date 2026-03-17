#include "be_constobj.h"

static be_define_const_map_slots(m_libpath_map) {
    { be_const_key(remove, 7), be_const_func(m_path_remove) },
    { be_const_key(exists, -1), be_const_func(m_path_exists) },
    { be_const_key(listdir, -1), be_const_func(m_path_listdir) },
    { be_const_key(rename, 0), be_const_func(m_path_rename) },
    { be_const_key(rmdir, -1), be_const_func(m_path_rmdir) },
    { be_const_key(last_modified, -1), be_const_func(m_path_last_modified) },
    { be_const_key(isdir, -1), be_const_func(m_path_isdir) },
    { be_const_key(mkdir, -1), be_const_func(m_path_mkdir) },
    { be_const_key(format, -1), be_const_func(m_path_format) },
};

static be_define_const_map(
    m_libpath_map,
    9
);

static be_define_const_module(
    m_libpath,
    "path"
);

BE_EXPORT_VARIABLE be_define_const_native_module(path);
