



























#ifndef PICOKDT_H_
#define PICOKDT_H_

#include "picoos.h"
#include "picoknow.h"





 















 


 
 
 
 

typedef enum {
    PICOKDT_KDTTYPE_POSP,
    PICOKDT_KDTTYPE_POSD,
    PICOKDT_KDTTYPE_G2P,
    PICOKDT_KDTTYPE_PHR,
    PICOKDT_KDTTYPE_ACC,
    PICOKDT_KDTTYPE_PAM
} picokdt_kdttype_t;

pico_status_t picokdt_specializeDtKnowledgeBase(picoknow_KnowledgeBase thiz,
                                                picoos_Common common,
                                                const picokdt_kdttype_t type);


 
 
 

 
typedef struct picokdt_dtposp * picokdt_DtPosP;
typedef struct picokdt_dtposd * picokdt_DtPosD;
typedef struct picokdt_dtg2p  * picokdt_DtG2P;
typedef struct picokdt_dtphr  * picokdt_DtPHR;
typedef struct picokdt_dtacc  * picokdt_DtACC;
typedef struct picokdt_dtpam  * picokdt_DtPAM;

 
picokdt_DtPosP picokdt_getDtPosP(picoknow_KnowledgeBase thiz);
picokdt_DtPosD picokdt_getDtPosD(picoknow_KnowledgeBase thiz);
picokdt_DtG2P  picokdt_getDtG2P (picoknow_KnowledgeBase thiz);
picokdt_DtPHR  picokdt_getDtPHR (picoknow_KnowledgeBase thiz);
picokdt_DtACC  picokdt_getDtACC (picoknow_KnowledgeBase thiz);
picokdt_DtPAM  picokdt_getDtPAM (picoknow_KnowledgeBase thiz);


 
typedef enum {
    PICOKDT_NRATT_POSP = 12,
    PICOKDT_NRATT_POSD =  7,
    PICOKDT_NRATT_G2P  = 16,
    PICOKDT_NRATT_PHR  =  8,
    PICOKDT_NRATT_ACC  = 13,
    PICOKDT_NRATT_PAM  = 60
} kdt_nratt_t;


 
 
 

typedef struct {
    picoos_uint8 set;     
    picoos_uint16 klass;
} picokdt_classify_result_t;


 
#define PICOKDT_MAXSIZE_OUTVEC 8

typedef struct {
    picoos_uint8 nr;     
    picoos_uint16 classvec[PICOKDT_MAXSIZE_OUTVEC];
} picokdt_classify_vecresult_t;


 
 
 
























 
 
 







#define PICOKDT_OUTSIDEGRAPH_DEFCH     (picoos_uint8)'\x30'    
#define PICOKDT_OUTSIDEGRAPH_DEFSTR  (picoos_uint8 *)PICO_S(15)
#define PICOKDT_OUTSIDEGRAPH_DEFLEN  1





#define PICOKDT_OUTSIDEGRAPH_EOW_DEFCH     (picoos_uint8)'\x31'  
#define PICOKDT_OUTSIDEGRAPH_EOW_DEFSTR  (picoos_uint8 *)PICO_S(16)
#define PICOKDT_OUTSIDEGRAPH_EOW_DEFLEN  1



#define PICOKDT_EPSILON  7









#define PICOKDT_HISTORY_ZERO  30000


 
 
 












picoos_uint8 picokdt_dtPosPconstructInVec(const picokdt_DtPosP thiz,
                                          const picoos_uint8 *graph,
                                          const picoos_uint16 graphlen,
                                          const picoos_uint8 specgraphflag);





picoos_uint8 picokdt_dtPosPclassify(const picokdt_DtPosP thiz);





picoos_uint8 picokdt_dtPosPdecomposeOutClass(const picokdt_DtPosP thiz,
                                             picokdt_classify_result_t *dtres);


 
 
 


















picoos_uint8 picokdt_dtPosDconstructInVec(const picokdt_DtPosD thiz,
                                          const picoos_uint16 * input);






picoos_uint8 picokdt_dtPosDclassify(const picokdt_DtPosD thiz,
                                    picoos_uint16 *treeout);





picoos_uint8 picokdt_dtPosDdecomposeOutClass(const picokdt_DtPosD thiz,
                                             picokdt_classify_result_t *dtres);

 
picoos_uint8 picokdt_dtPosDreverseMapOutFixed(const picokdt_DtPosD thiz,
                                          const picoos_uint16 inval,
                                          picoos_uint16 *outval,
                                          picoos_uint16 *outfallbackval);

 
 
 



















picoos_uint8 picokdt_dtG2PconstructInVec(const picokdt_DtG2P thiz,
                                         const picoos_uint8 *graph,
                                         const picoos_uint16 graphlen,
                                         const picoos_uint8 count,
                                         const picoos_uint8 pos,
                                         const picoos_uint8 nrvow,
                                         const picoos_uint8 ordvow,
                                         picoos_uint8 *primstressflag,
                                         const picoos_uint16 phonech1,
                                         const picoos_uint16 phonech2,
                                         const picoos_uint16 phonech3);





picoos_uint8 picokdt_dtG2Pclassify(const picokdt_DtG2P thiz,
                                   picoos_uint16 *treeout);





picoos_uint8 picokdt_dtG2PdecomposeOutClass(const picokdt_DtG2P thiz,
                                  picokdt_classify_vecresult_t *dtvres);


 
 
 





















picoos_uint8 picokdt_dtPHRconstructInVec(const picokdt_DtPHR thiz,
                                         const picoos_uint8 pre2,
                                         const picoos_uint8 pre1,
                                         const picoos_uint8 src,
                                         const picoos_uint8 fol1,
                                         const picoos_uint8 fol2,
                                         const picoos_uint16 nrwordspre,
                                         const picoos_uint16 nrwordsfol,
                                         const picoos_uint16 nrsyllsfol);




picoos_uint8 picokdt_dtPHRclassify(const picokdt_DtPHR thiz);





picoos_uint8 picokdt_dtPHRdecomposeOutClass(const picokdt_DtPHR thiz,
                                            picokdt_classify_result_t *dtres);


 
 
 





























picoos_uint8 picokdt_dtACCconstructInVec(const picokdt_DtACC thiz,
                                         const picoos_uint8 pre2,
                                         const picoos_uint8 pre1,
                                         const picoos_uint8 src,
                                         const picoos_uint8 fol1,
                                         const picoos_uint8 fol2,
                                         const picoos_uint16 hist1,
                                         const picoos_uint16 hist2,
                                         const picoos_uint16 nrwordspre,
                                         const picoos_uint16 nrsyllspre,
                                         const picoos_uint16 nrwordsfol,
                                         const picoos_uint16 nrsyllsfol,
                                         const picoos_uint16 footwordsfol,
                                         const picoos_uint16 footsyllsfol);





picoos_uint8 picokdt_dtACCclassify(const picokdt_DtACC thiz,
                                   picoos_uint16 *treeout);





picoos_uint8 picokdt_dtACCdecomposeOutClass(const picokdt_DtACC thiz,
                                            picokdt_classify_result_t *dtres);


 
 
 







picoos_uint8 picokdt_dtPAMconstructInVec(const picokdt_DtPAM thiz,
                                         const picoos_uint8 *vec,
                                         const picoos_uint8 veclen);




picoos_uint8 picokdt_dtPAMclassify(const picokdt_DtPAM thiz);





picoos_uint8 picokdt_dtPAMdecomposeOutClass(const picokdt_DtPAM thiz,
                                            picokdt_classify_result_t *dtres);





#endif  
