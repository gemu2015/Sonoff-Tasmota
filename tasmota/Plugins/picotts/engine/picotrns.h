


















































#ifndef PICOTRNS_H_
#define PICOTRNS_H_

#include "picoos.h"
#include "picokfst.h"
#include "picoktab.h"




#define PICOTRNS_MAX_NUM_POSSYM 255

#define PICOTRNS_POS_INSERT   (picoos_int16) -1     
#define PICOTRNS_POS_INVALID  (picoos_int16) -2     
#define PICOTRNS_POS_IGNORE   (picoos_int16) -3     


typedef struct picotrns_possym {
    picoos_int16 pos;
    picoos_int16 sym;
} picotrns_possym_t;

picoos_uint8 picotrns_unplane(picoos_int16 symIn, picoos_uint8 * plane);


#if defined(PICO_DEBUG)

void PICOTRNS_PRINTSYM(picoknow_KnowledgeBase kbdbg, picoos_int16 insym);

void PICOTRNS_PRINTSYMSEQ(picoknow_KnowledgeBase kbdbg, const picotrns_possym_t seq[], const picoos_uint16 seqLen);

void picotrns_printSolution(const picotrns_possym_t outSeq[], const picoos_uint16 outSeqLen);

#else
#define PICOTRNS_PRINTSYM(x,y)
#define PICOTRNS_PRINTSYMSEQ(x,y,z)
#define picotrns_printSolution NULL
#endif


typedef struct picotrns_altDesc * picotrns_AltDesc;


picotrns_AltDesc picotrns_allocate_alt_desc_buf(picoos_MemoryManager mm, picoos_uint32 maxByteSize, picoos_uint16 * numAltDescs);

void picotrns_deallocate_alt_desc_buf(picoos_MemoryManager mm, picotrns_AltDesc * altDescBuf);





typedef void picotrns_printSolutionFct(const picotrns_possym_t outSeq[], const picoos_uint16 outSeqLen);

























extern pico_status_t picotrns_transduce (picokfst_FST fst, picoos_bool firstSolOnly,
                                         picotrns_printSolutionFct printSolution,
                                         const picotrns_possym_t inSeq[], picoos_uint16 inSeqLen,
                                         picotrns_possym_t outSeq[], picoos_uint16 * outSeqLen, picoos_uint16 maxOutSeqLen,
                                         picotrns_AltDesc altDescBuf, picoos_uint16 maxAltDescLen,
                                         picoos_uint32 *nrSteps);



 





 
pico_status_t picotrns_eliminate_epsilons(const picotrns_possym_t inSeq[], picoos_uint16 inSeqLen,
        picotrns_possym_t outSeq[], picoos_uint16 * outSeqLen, picoos_uint16 maxOutSeqLen);



pico_status_t picotrns_trivial_syllabify(picoktab_Phones phones,
        const picotrns_possym_t inSeq[], const picoos_uint16 inSeqLen,
        picotrns_possym_t outSeq[], picoos_uint16 * outSeqLen, picoos_uint16 maxOutSeqLen);






typedef struct picotrns_simple_transducer * picotrns_SimpleTransducer;

picotrns_SimpleTransducer picotrns_newSimpleTransducer(picoos_MemoryManager mm,
                                              picoos_Common common,
                                              picoos_uint16 maxAltDescLen);

pico_status_t picotrns_disposeSimpleTransducer(picotrns_SimpleTransducer * thiz,
        picoos_MemoryManager mm);

pico_status_t  picotrns_stInitialize(picotrns_SimpleTransducer transducer);

pico_status_t picotrns_stAddWithPlane(picotrns_SimpleTransducer thiz, picoos_char * inStr, picoos_uint8 plane);

pico_status_t picotrns_stTransduce(picotrns_SimpleTransducer thiz, picokfst_FST fst);

pico_status_t picotrns_stGetSymSequence(
        picotrns_SimpleTransducer thiz,
        picoos_uint8 * outputSymIds,
        picoos_uint32 maxOutputSymIds);







#endif  
