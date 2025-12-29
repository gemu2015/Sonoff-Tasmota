#ifndef L3SUBBAND_H
#define L3SUBBAND_H

#include <stdint.h>

MODULE_PART void p_shine_subband_initialise( shine_global_config *config );
MODULE_PART void p_shine_window_filter_subband(int16_t **buffer, int32_t s[SBLIMIT], int32_t k, shine_global_config *config, int32_t stride);

#endif
