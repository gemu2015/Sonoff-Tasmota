#include "be_constobj.h"

static be_define_const_map_slots(m_liblv_tasmota_map) {
    { be_const_key_weak(load_freetype_font, -1), be_const_func(lv0_load_freetype_font) },
    { be_const_key_weak(font_embedded, -1), be_const_func(lv0_load_font_embedded) },
    { be_const_key_weak(start, 1), be_const_func(lv0_start) },
    { be_const_key_weak(seg7_font, -1), be_const_func(lv0_load_seg7_font) },
    { be_const_key_weak(font_seg7, -1), be_const_func(lv0_load_seg7_font) },
    { be_const_key_weak(init, 4), be_const_closure(module_lv_tasmota_init_closure) },
    { be_const_key_weak(font_montserrat, -1), be_const_func(lv0_load_montserrat_font) },
    { be_const_key_weak(splash_remove, 2), be_const_closure(module_lv_tasmota_splash_remove_closure) },
    { be_const_key_weak(register_button_encoder, -1), be_const_func(lv0_register_button_encoder) },
    { be_const_key_weak(_constants, -1), be_const_func(lv0_constants_as_hash) },
    { be_const_key_weak(montserrat_font, -1), be_const_func(lv0_load_montserrat_font) },
    { be_const_key_weak(splash_init, -1), be_const_closure(module_lv_tasmota_splash_init_closure) },
    { be_const_key_weak(splash, -1), be_const_closure(module_lv_tasmota_splash_closure) },
    { be_const_key_weak(screenshot, 7), be_const_func(lv0_screenshot) },
};

static be_define_const_map(
    m_liblv_tasmota_map,
    14
);

static be_define_const_module(
    m_liblv_tasmota,
    "lv_tasmota"
);

BE_EXPORT_VARIABLE be_define_const_native_module(lv_tasmota);
