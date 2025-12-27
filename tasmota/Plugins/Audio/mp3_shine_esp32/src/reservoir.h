#ifndef RESERVOIR_H
#define RESERVOIR_H

void p_shine_ResvFrameBegin(int frameLength, p_shine_global_config *config);
int  p_shine_max_reservoir_bits   (double *pe, p_shine_global_config *config);
void p_shine_ResvAdjust    (gr_info *gi, p_shine_global_config *config );
void p_shine_ResvFrameEnd  (p_shine_global_config *config );

#endif
