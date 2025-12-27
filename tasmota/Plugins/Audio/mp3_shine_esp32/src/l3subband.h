#ifndef L3SUBBAND_H
#define L3SUBBAND_H

#include <stdint.h>

void p_shine_subband_initialise( p_shine_global_config *config );
void p_shine_window_filter_subband(int16_t **buffer, int s[SBLIMIT], int k, p_shine_global_config *config, int stride);

#endif
