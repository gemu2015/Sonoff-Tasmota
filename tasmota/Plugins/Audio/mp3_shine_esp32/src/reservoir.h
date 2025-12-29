#ifndef RESERVOIR_H
#define RESERVOIR_H

MODULE_PART void p_shine_ResvFrameBegin(int32_t frameLength, shine_global_config *config);
MODULE_PART int32_t  p_shine_max_reservoir_bits   (SHINE_DOUBLE *pe, shine_global_config *config);
MODULE_PART void p_shine_ResvAdjust    (gr_info *gi, shine_global_config *config );
MODULE_PART void p_shine_ResvFrameEnd  (shine_global_config *config );

#endif
