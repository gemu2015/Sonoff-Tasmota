

































#ifndef PICOKTAB_H_
#define PICOKTAB_H_

#include "picoos.h"
#include "picoknow.h"





 
 
 




typedef struct picoktab_fixed_ids * picoktab_FixedIds;

typedef struct picoktab_fixed_ids {
    picoos_uint8 phonStartId;
    picoos_uint8 phonTermId;
} picoktab_fixed_ids_t;

 
pico_status_t picoktab_specializeIdsKnowledgeBase(picoknow_KnowledgeBase thiz,
                                                  picoos_Common common);

picoktab_FixedIds picoktab_getFixedIds(picoknow_KnowledgeBase thiz);


 
 
 

typedef struct picoktab_graphs *picoktab_Graphs;

 
pico_status_t picoktab_specializeGraphsKnowledgeBase(picoknow_KnowledgeBase thiz,
                                                     picoos_Common common);

 
picoktab_Graphs picoktab_getGraphs(picoknow_KnowledgeBase thiz);




picoos_uint32 picoktab_graphOffset(const picoktab_Graphs thiz,
                                   picoos_uchar * utf8graph);




picoos_uint8 picoktab_hasVowellikeProp(const picoktab_Graphs thiz,
                                       const picoos_uint8 *graph,
                                       const picoos_uint8 graphlenmax);



picoos_bool  picoktab_getIntPropTokenType(const picoktab_Graphs thiz,
                                           picoos_uint32 graphsOffset,
                                           picoos_uint8 *stokenType);
picoos_bool  picoktab_getIntPropTokenSubType(const picoktab_Graphs thiz,
                                              picoos_uint32 graphsOffset,
                                              picoos_int8 *stokenSubType);
picoos_bool  picoktab_getIntPropValue(const picoktab_Graphs thiz,
                                      picoos_uint32 graphsOffset,
                                      picoos_uint32 *value);
picoos_bool  picoktab_getStrPropLowercase(const picoktab_Graphs thiz,
                                          picoos_uint32 graphsOffset,
                                          picoos_uchar *lowercase);
picoos_bool  picoktab_getStrPropGraphsubs1(const picoktab_Graphs thiz,
                                           picoos_uint32 graphsOffset,
                                           picoos_uchar *graphsubs1);
picoos_bool  picoktab_getStrPropGraphsubs2(const picoktab_Graphs thiz,
                                           picoos_uint32 graphsOffset,
                                           picoos_uchar *graphsubs2);
picoos_bool  picoktab_getIntPropPunct(const picoktab_Graphs thiz,
                                      picoos_uint32 graphsOffset,
                                      picoos_uint8 *info1,
                                      picoos_uint8 *info2);

picoos_uint16 picoktab_graphsGetNumEntries(const picoktab_Graphs thiz);
void picoktab_graphsGetGraphInfo(const picoktab_Graphs thiz,
        picoos_uint16 graphIndex, picoos_uchar * from, picoos_uchar * to,
        picoos_uint8 * propset,
        picoos_uint8 * stokenType, picoos_uint8 * stokenSubType,
        picoos_uint8 * value, picoos_uchar * lowercase,
        picoos_uchar * graphsubs1, picoos_uchar * graphsubs2,
        picoos_uint8 * punct);


 
 
 

 
pico_status_t picoktab_specializePhonesKnowledgeBase(picoknow_KnowledgeBase thiz,
                                                     picoos_Common common);

typedef struct picoktab_phones *picoktab_Phones;

 
picoktab_Phones picoktab_getPhones(picoknow_KnowledgeBase thiz);



picoos_uint8 picoktab_hasVowelProp(const picoktab_Phones thiz,
                                   const picoos_uint8 ch);
picoos_uint8 picoktab_hasDiphthProp(const picoktab_Phones thiz,
                                    const picoos_uint8 ch);
picoos_uint8 picoktab_hasGlottProp(const picoktab_Phones thiz,
                                   const picoos_uint8 ch);
picoos_uint8 picoktab_hasNonsyllvowelProp(const picoktab_Phones thiz,
                                          const picoos_uint8 ch);
picoos_uint8 picoktab_hasSyllconsProp(const picoktab_Phones thiz,
                                      const picoos_uint8 ch);




picoos_bool picoktab_isSyllCarrier(const picoktab_Phones thiz,
                                    const picoos_uint8 ch);




picoos_bool picoktab_isPrimstress(const picoktab_Phones thiz,
                                   const picoos_uint8 ch);
picoos_bool picoktab_isSecstress(const picoktab_Phones thiz,
                                  const picoos_uint8 ch);
picoos_bool picoktab_isSyllbound(const picoktab_Phones thiz,
                                  const picoos_uint8 ch);
picoos_bool picoktab_isWordbound(const picoktab_Phones thiz,
                                  const picoos_uint8 ch);
picoos_bool picoktab_isPause(const picoktab_Phones thiz,
                              const picoos_uint8 ch);

 
picoos_uint8 picoktab_getPrimstressID(const picoktab_Phones thiz);
picoos_uint8 picoktab_getSecstressID(const picoktab_Phones thiz);
picoos_uint8 picoktab_getSyllboundID(const picoktab_Phones thiz);
picoos_uint8 picoktab_getWordboundID(const picoktab_Phones thiz);
picoos_uint8 picoktab_getPauseID(const picoktab_Phones thiz);

 
 
 

 
pico_status_t picoktab_specializePosKnowledgeBase(picoknow_KnowledgeBase thiz,
                                                  picoos_Common common);

typedef struct picoktab_pos *picoktab_Pos;

#define PICOKTAB_MAXNRPOS_IN_COMB  8

 
picoktab_Pos picoktab_getPos(picoknow_KnowledgeBase thiz);



picoos_bool picoktab_isUniquePos(const picoktab_Pos thiz,
                                  const picoos_uint8 pos);





picoos_bool picoktab_isPartOfPosGroup(const picoktab_Pos thiz,
                                       const picoos_uint8 pos,
                                       const picoos_uint8 posgroup);




picoos_uint8 picoktab_getPosGroup(const picoktab_Pos thiz,
                                  const picoos_uint8 *poslist,
                                  const picoos_uint8 poslistlen);




#endif  
