#include "be_constobj.h"

static be_define_const_map_slots(m_libanimate_map) {
    { be_const_key_weak(animator, -1), be_const_class(be_class_Animate_animator) },
    { be_const_key_weak(frame, -1), be_const_class(be_class_Leds_frame) },
    { be_const_key_weak(core, -1), be_const_class(be_class_Animate_core) },
    { be_const_key_weak(PALETTE_ib_44, 9), be_const_comptr(PALETTE_ib_44) },
    { be_const_key_weak(SAWTOOTH, -1), be_const_int(1) },
    { be_const_key_weak(PALETTE_Fire_1, -1), be_const_comptr(PALETTE_Fire_1) },
    { be_const_key_weak(SQUARE, -1), be_const_int(3) },
    { be_const_key_weak(LASTFORM, 10), be_const_int(5) },
    { be_const_key_weak(PALETTE_bhw1_sunconure, 15), be_const_comptr(PALETTE_bhw1_sunconure) },
    { be_const_key_weak(oscillator, -1), be_const_class(be_class_Animate_oscillator) },
    { be_const_key_weak(PALETTE_SATURATED_TAG, -1), be_const_comptr(PALETTE_SATURATED_TAG) },
    { be_const_key_weak(PALETTE_RAINBOW_WHITE, 19), be_const_comptr(PALETTE_RAINBOW_WHITE) },
    { be_const_key_weak(PALETTE_STANDARD_VAL, 0), be_const_comptr(PALETTE_STANDARD_VAL) },
    { be_const_key_weak(TRIANGLE, -1), be_const_int(2) },
    { be_const_key_weak(COSINE, 1), be_const_int(4) },
    { be_const_key_weak(PALETTE_bhw4_089, -1), be_const_comptr(PALETTE_bhw4_089) },
    { be_const_key_weak(palette, 4), be_const_class(be_class_Animate_palette) },
    { be_const_key_weak(PALETTE_ib_jul01_gp, -1), be_const_comptr(PALETTE_ib_jul01_gp) },
    { be_const_key_weak(pulse, -1), be_const_class(be_class_Animate_pulse) },
    { be_const_key_weak(PALETTE_STANDARD_TAG, -1), be_const_comptr(PALETTE_STANDARD_TAG) },
};

static be_define_const_map(
    m_libanimate_map,
    20
);

static be_define_const_module(
    m_libanimate,
    "animate"
);

BE_EXPORT_VARIABLE be_define_const_native_module(animate);
