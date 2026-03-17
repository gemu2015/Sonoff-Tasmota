#include "be_constobj.h"

static be_define_const_map_slots(m_libflash_map) {
    { be_const_key(write, -1), be_const_func(p_flash_write) },
    { be_const_key(current_ota, -1), be_const_ctype_func(p_cur_ota) },
    { be_const_key(factory, 1), be_const_ctype_func(p_factory) },
    { be_const_key(id, 5), be_const_ctype_func(p_flashid) },
    { be_const_key(erase, -1), be_const_func(p_flash_erase) },
    { be_const_key(read, -1), be_const_func(p_flash_read) },
    { be_const_key(size, 4), be_const_ctype_func(p_flashsize) },
};

static be_define_const_map(
    m_libflash_map,
    7
);

static be_define_const_module(
    m_libflash,
    "flash"
);

BE_EXPORT_VARIABLE be_define_const_native_module(flash);
