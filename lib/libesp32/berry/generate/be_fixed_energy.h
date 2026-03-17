#include "be_constobj.h"

static be_define_const_map_slots(m_libenergy_map) {
    { be_const_key(tomap, -1), be_const_closure(module_energy_tomap_closure) },
    { be_const_key(init, -1), be_const_closure(module_energy_init_closure) },
    { be_const_key(_deref, -1), be_const_closure(module_energy__deref_closure) },
    { be_const_key(read, 6), be_const_closure(module_energy_read_closure) },
    { be_const_key(member, 11), be_const_closure(module_energy_member_closure) },
    { be_const_key(setmember, 1), be_const_closure(module_energy_setmember_closure) },
    { be_const_key(_phases_uint8, -1), be_const_class(be_class_energy_phases_uint8) },
    { be_const_key(_phases_float, -1), be_const_class(be_class_energy_phases_float) },
    { be_const_key(_ptr, -1), be_const_comptr(&Energy) },
    { be_const_key(_phases_int32, -1), be_const_class(be_class_energy_phases_int32) },
    { be_const_key(_phases_uint16, -1), be_const_class(be_class_energy_phases_uint16) },
    { be_const_key(update_total, -1), be_const_func(module_energy_update_total) },
    { be_const_key(_phases, 8), be_const_int(ENERGY_MAX_PHASES) },
    { be_const_key(driver_enabled, 4), be_const_func(module_energy_driver_enabled) },
};

static be_define_const_map(
    m_libenergy_map,
    14
);

static be_define_const_module(
    m_libenergy,
    "energy"
);

BE_EXPORT_VARIABLE be_define_const_native_module(energy);
