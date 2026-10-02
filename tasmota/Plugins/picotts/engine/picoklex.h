



























#ifndef PICOKLEX_H_
#define PICOKLEX_H_

#include "picoos.h"
#include "picoknow.h"





 
 
 
 

pico_status_t picoklex_specializeLexKnowledgeBase(picoknow_KnowledgeBase thiz,
                                                  picoos_Common common);


 
 
 

 
typedef struct picoklex_lex * picoklex_Lex;

 
picoklex_Lex picoklex_getLex(picoknow_KnowledgeBase thiz);


 
 
 

 
#define PICOKLEX_MAX_NRRES   4

 
#define PICOKLEX_POSIND_SIZE 4
 
#define PICOKLEX_IND_SIZE    3
 
#define PICOKLEX_POSIND_MAXLEN 16













typedef struct {
    picoos_uint8 nrres;       
    picoos_uint8 posindlen;   
    picoos_uint8 phonfound;   
    picoos_uint8 posind[PICOKLEX_POSIND_MAXLEN]; 

} picoklex_lexl_result_t;


 
 
 








picoos_uint8 picoklex_lexLookup(const picoklex_Lex thiz,
                                const picoos_uint8 *graph,
                                const picoos_uint16 graphlen,
                                picoklex_lexl_result_t *lexres);




picoos_uint8 picoklex_lexIndLookup(const picoklex_Lex thiz,
                                   const picoos_uint8 *ind,
                                   const picoos_uint8 indlen,
                                   picoos_uint8 *pos,
                                   picoos_uint8 **phon,
                                   picoos_uint8 *phonlen);




#endif  
