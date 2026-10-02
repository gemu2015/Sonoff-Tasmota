



























#ifndef PICODSP_H_
#define PICODSP_H_




 
 
#define PICODSP_START_FLOAT_NORM      0.41f
#define PICODSP_ENVSPEC_K1            0.5f
#define PICODSP_ENVSPEC_K2            2
#define PICODSP_GETEXC_K1             1024
#define PICODSP_FIXRESP_NORM          4096.0f
#define PICODSP_END_FLOAT_NORM        1.5f*16.0f
#define PICODSP_FIX_SCALE1            0x4000000
#define PICODSP_FIX_SCALE2            0x4000
#define PICODSP_SHIFT_FACT1           10
#define PICODSP_SHIFT_FACT2           16
#define PICODSP_SHIFT_FACT3           12
#define PICODSP_SHIFT_FACT4           1
#define PICODSP_SHIFT_FACT5           18
#define PICODSP_SHIFT_FACT6           9
#define PICOSIG_NORM1                 9.14f /100.0f  
#define PICOSIG_MAXAMP                (32767)
#define PICOSIG_MINAMP                (-32768)
#define PICODSP_M_PI        3.14159265358979323846
#define PICODSP_MAX_EX      32
#define PICODSP_WGT_SHIFT  (0x20000000)
#define PICODSP_N_RAND_TABLE (760)
#define PICODSP_COS_TABLE_LEN (512)
#define PICODSP_COS_TABLE_LEN2 (1024)
#define PICODSP_COS_TABLE_LEN4 (2048)
#define PICODSP_PI_SHIFT (4)             

#define PICODSP_V_CUTOFF_FREQ  4500
#define PICODSP_UV_CUTOFF_FREQ 300
#define PICODSP_SAMP_FREQ      16000
#define PICODSP_FREQ_WARP_FACT 0.42f

 
#define PICODSP_CEPORDER    25
#define PICODSP_PHASEORDER  72
#define CEPST_BUFF_SIZE     3
#define PHASE_BUFF_SIZE     5
 
#define PICODSP_FFTSIZE     (256)

#define PICODSP_H_FFTSIZE   (PICODSP_FFTSIZE/2)

#define PICODSP_DISPLACE    PICODSP_FFTSIZE/4

#define PICODSP_H_FFTSIZE   (PICODSP_FFTSIZE/2)
#define PICODSP_HFFTSIZE_P1 (PICODSP_H_FFTSIZE+1)

#define FAST_DEVICE(aCount, aAction) \
{ \
    int count_ = (aCount); \
    int times_ = (count_ + 7) >> 3; \
    switch (count_ & 7){ \
        case 0: do { aAction; \
        case 7: aAction; \
        case 6: aAction; \
        case 5: aAction; \
        case 4: aAction; \
        case 3: aAction; \
        case 2: aAction; \
        case 1: aAction; \
    } while (--times_ > 0); \
} \
}



#define EXP(y) picoos_quick_exp(y)



#endif  
