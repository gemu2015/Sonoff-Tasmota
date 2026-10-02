



























#ifndef PICOKPDF_H_
#define PICOKPDF_H_

#include "picoos.h"
#include "picoknow.h"





 











 


 
 
 
 

#define PICOKPDF_MAX_NUM_STATES 10

#define PICOKPDF_MAX_MUL_LFZ_CEPORDER 1
#define PICOKPDF_MAX_MUL_MGC_CEPORDER 25





#define PICOKPDF_BIG_POW 12

typedef enum {
    PICOKPDF_KPDFTYPE_DUR,
    PICOKPDF_KPDFTYPE_MUL,
    PICOKPDF_KPDFTYPE_PHS
} picokpdf_kpdftype_t;

pico_status_t picokpdf_specializePdfKnowledgeBase(picoknow_KnowledgeBase thiz,
                                              picoos_Common common,
                                              const picokpdf_kpdftype_t type);


 
 
 






typedef struct picokpdf_pdfdur *picokpdf_PdfDUR;
typedef struct picokpdf_pdfmul *picokpdf_PdfMUL;
typedef struct picokpdf_pdfphs *picokpdf_PdfPHS;

 
typedef struct picokpdf_pdfdur {
    picoos_uint16 numframes;
    picoos_uint8 vecsize;
    picoos_uint8 sampperframe;
    picoos_uint8 phonquantlen;
    picoos_uint8 *phonquant;
    picoos_uint8 statequantlen;
    picoos_uint8 *statequant;
    picoos_uint8 *content;
} picokpdf_pdfdur_t;

 
typedef struct picokpdf_pdfmul {
    picoos_uint16 numframes;
    picoos_uint8 vecsize;
    picoos_uint8 numstates;
    picoos_uint16 stateoffset[PICOKPDF_MAX_NUM_STATES];  
    picoos_uint8 ceporder;
    picoos_uint8 numvuv;
    picoos_uint8 numdeltas;
    picoos_uint8 meanpow;
    picoos_uint8 bigpow;
    picoos_uint8 amplif;
    picoos_uint8 *meanpowUm;   
    picoos_uint8 *ivarpow;     
    picoos_uint8 *content;
} picokpdf_pdfmul_t;

 
typedef struct picokpdf_pdfphs {
    picoos_uint16 numvectors;
    picoos_uint8 *indexBase;
    picoos_uint8 *contentBase;
} picokpdf_pdfphs_t;

 
picokpdf_PdfDUR picokpdf_getPdfDUR(picoknow_KnowledgeBase thiz);
picokpdf_PdfMUL picokpdf_getPdfMUL(picoknow_KnowledgeBase thiz);
picokpdf_PdfPHS picokpdf_getPdfPHS(picoknow_KnowledgeBase thiz);


 
 
 

 






 
 
 




#endif  
