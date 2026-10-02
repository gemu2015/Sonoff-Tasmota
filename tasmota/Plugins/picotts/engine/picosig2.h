



























#ifndef PICOSIG2_H_
#define PICOSIG2_H_

#include "picoos.h"
#include "picodsp.h"









typedef struct sig_innerobj
{

     
    picoos_int16 *idx_vect1;  
    picoos_int16 *idx_vect2;  
    picoos_int16 *idx_vect4;  
    picoos_int16 *idx_vect5;  
    picoos_int16 *idx_vect6;  
    picoos_int16 *idx_vect7;  
    picoos_int16 *idx_vect8;  
    picoos_int16 *idx_vect9;  

    picoos_int32 *int_vec22;  
    picoos_int32 *int_vec23;  
    picoos_int32 *int_vec24;  
    picoos_int32 *int_vec25;  
    picoos_int32 *int_vec26;  
    picoos_int32 *int_vec28;  
    picoos_int32 *int_vec29;  
    picoos_int32 *int_vec38;  
    picoos_int32 *int_vec30;  
    picoos_int32 *int_vec31;  

    picoos_int32 *int_vec32;  
    picoos_int32 *int_vec33;  

    picoos_int32 *int_vec34;  
    picoos_int32 *int_vec35;  
    picoos_int32 *int_vec36;  
    picoos_int32 *int_vec37;  

    picoos_int32 *int_vec39;  
    picoos_int32 *int_vec40;  

    picoos_int32 *int_vec41[CEPST_BUFF_SIZE];  
    picoos_int32 *int_vec42[PHASE_BUFF_SIZE];  

    picoos_int16 idx_vect10[CEPST_BUFF_SIZE];  
    picoos_int16 idx_vect11[CEPST_BUFF_SIZE];  
    picoos_int16 idx_vect12[CEPST_BUFF_SIZE];  
    picoos_int16 idx_vect13[CEPST_BUFF_SIZE];  
    picoos_int16 idx_vect14[PHASE_BUFF_SIZE];  

    picoos_int32 *sig_vec1;

    picoos_single bvalue1;  
    picoos_int32 ibvalue2;  
    picoos_int32 ibvalue3;  
    picoos_single bvalue4;  
    picoos_single bvalue5;  
    picoos_single bvalue6;  

    picoos_single bvalue7;  
    picoos_single bvalue8;  

    picoos_int16 ivalue1;  
    picoos_int16 ivalue2;  
    picoos_int16 ivalue3;  
    picoos_int16 ivalue4;  
    picoos_int16 ivalue5;  
    picoos_int16 ivalue6;  
    picoos_int16 ivalue7;  
    picoos_int16 ivalue8;  
    picoos_int16 ivalue9;  
    picoos_int16 ivalue10;  
    picoos_int16 ivalue11;  
    picoos_int16 ivalue12;  
    picoos_int16 ivalue13;  
    picoos_int16 ivalue14;  
    picoos_int16 ivalue15;  
    picoos_int16 ivalue16;  
    picoos_int16 ivalue17;  
    picoos_int16 ivalue18;  

    picoos_int16 ivalue19;  

    picoos_int16 ivalue20;  

    picoos_int32 lvalue1;  
    picoos_int32 lvalue2;  
    picoos_int32 lvalue3;  
    picoos_int32 lvalue4;  

    picoos_int32 iRand;  

} sig_innerobj_t;





extern pico_status_t sigAllocate(picoos_MemoryManager mm,
        sig_innerobj_t *sig_inObj);
extern void sigDeallocate(picoos_MemoryManager mm, sig_innerobj_t *sig_inObj);
extern void sigDspInitialize(sig_innerobj_t *sig_inObj, picoos_int32 resetMode);





extern void mel_2_lin_init(sig_innerobj_t *sig_inObj);
extern void save_transition_frame(sig_innerobj_t *sig_inObj);
extern void mel_2_lin_init(sig_innerobj_t *sig_inObj);
extern void post_filter_init(sig_innerobj_t *sig_inObj);
extern void mel_2_lin_lookup(sig_innerobj_t *sig_inObj, picoos_uint32 mgc);
extern void post_filter(sig_innerobj_t *sig_inObj);
extern void phase_spec2(sig_innerobj_t *sig_inObj);
extern void env_spec(sig_innerobj_t *sig_inObj);
extern void save_transition_frame(sig_innerobj_t *sig_inObj);
extern void td_psola2(sig_innerobj_t *sig_inObj);
extern void impulse_response(sig_innerobj_t *sig_inObj);
extern void overlap_add(sig_innerobj_t *sig_inObj);




#define WavBuff_p   int_vec26        
#define window_p    int_vec25        
#define ImpResp_p   int_vec23        
#define imp_p       int_vec24        
#define warp_p      bvalue1          
#define voxbnd_p    ibvalue2             
#define voxbnd2_p   ibvalue3             
#define E_p         bvalue4          
#define F0_p        bvalue5          
#define sMod_p      bvalue6          
#define voicing     bvalue7          
#define Fuv_p       bvalue8          
#define m1_p        ivalue1          
#define m2_p        ivalue2          
#define windowLen_p ivalue2          
#define hfftsize_p  ivalue3          
#define framesz_p   ivalue4          
#define voiced_p    ivalue5          
#define nRes_p      ivalue6          
#define i_p         ivalue7          
#define j_p         ivalue8          
#define hop_p       ivalue9          
#define nextPeak_p  ivalue10         
#define phId_p      ivalue14         
#define prevVoiced_p ivalue16         
#define nV          ivalue17
#define nU          ivalue18
#define VoicTrans   ivalue19         
#define Fs_p        lvalue1          
#define VCutoff_p   lvalue2          
#define UVCutoff_p  lvalue3          
 
#define wcep_pI     int_vec28        
#define d_p         int_vec38        
#define A_p         idx_vect2        
#define ang_p       int_vec39        
#define EnV         int_vec30
#define EnU         int_vec31
#define randCosTbl  int_vec34
#define randSinTbl  int_vec35
#define outCosTbl   int_vec36
#define outSinTbl   int_vec37
#define cos_table   int_vec40
#define norm_window_p int_vec22      
#define norm_window2_p int_vec27     
#define F2r_p       int_vec32        
#define F2i_p       int_vec33        
#define LocV        idx_vect8        
#define LocU        idx_vect9        

#define CepBuff       int_vec41      
#define PhsBuff       int_vec42      
#define F0Buff        idx_vect10     
#define PhIdBuff      idx_vect11     
#define VoicingBuff   idx_vect12     
#define FuVBuff       idx_vect13     
#define VoxBndBuff    idx_vect14     

#define n_available   ivalue20       




#endif
