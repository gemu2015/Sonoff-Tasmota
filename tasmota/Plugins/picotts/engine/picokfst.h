


























#ifndef PICOKFST_H_
#define PICOKFST_H_

#include "picodefs.h"
#include "picodbg.h"
#include "picoos.h"
#include "picoknow.h"




typedef picoos_int16 picokfst_symid_t;  
typedef picoos_int16 picokfst_class_t;  
typedef picoos_int16 picokfst_state_t;  

#define PICOKFST_SYMID_EPS    (picokfst_symid_t)   0    
#define PICOKFST_SYMID_ILLEG  (picokfst_symid_t)  -1    













enum picokfst_symbol_plane {
    PICOKFST_PLANE_PHONEMES = 0,        
    PICOKFST_PLANE_ASCII = 1,           
    PICOKFST_PLANE_XSAMPA = 2,          
    PICOKFST_PLANE_ACCENTS = 4,         
    PICOKFST_PLANE_POS = 5,             
    PICOKFST_PLANE_PB_STRENGTHS = 6,    
    PICOKFST_PLANE_INTERN = 7           
};




enum picofst_transduction_mode {
    PICOKFST_TRANSMODE_NEWSYMS = 1,  
    PICOKFST_TRANSMODE_POSUSED = 2  

};


 
 
 
 




pico_status_t picokfst_specializeFSTKnowledgeBase(picoknow_KnowledgeBase thiz,
                                                  picoos_Common common);


 
 
 

 
typedef struct picokfst_fst * picokfst_FST;

 
picokfst_FST picokfst_getFST(picoknow_KnowledgeBase thiz);


 
 
 



picoos_uint8 picokfst_kfstGetTransductionMode(picokfst_FST thiz);



void picokfst_kfstGetFSTSizes (picokfst_FST thiz, picoos_int32 *nrStates, picoos_int32 *nrClasses);





void picokfst_kfstStartPairSearch (picokfst_FST thiz, picokfst_symid_t inSym,
                                          picoos_bool * inSymFound, picoos_int32 * searchState);





void picokfst_kfstGetNextPair (picokfst_FST thiz, picoos_int32 * searchState,
                                      picoos_bool * pairFound,
                                      picokfst_symid_t * outSym, picokfst_class_t * pairClass);




void picokfst_kfstGetTrans (picokfst_FST thiz, picokfst_state_t startState, picokfst_class_t transClass,
                                   picokfst_state_t * endState);







void picokfst_kfstStartInEpsTransSearch (picokfst_FST thiz, picokfst_state_t startState,
                                                picoos_bool * inEpsTransFound, picoos_int32 * searchState);







void picokfst_kfstGetNextInEpsTrans (picokfst_FST thiz, picoos_int32 * searchState,
                                            picoos_bool * inEpsTransFound,
                                            picokfst_symid_t * outSym, picokfst_state_t * endState);






picoos_bool picokfst_kfstIsAcceptingState (picokfst_FST thiz, picokfst_state_t state);




#endif  
