































#ifndef PICOKNOW_H_
#define PICOKNOW_H_

#include "picodefs.h"
#include "picoos.h"





typedef enum picoknow_kb_id {
    PICOKNOW_KBID_NULL         = 0,
     
    PICOKNOW_KBID_TPP_MAIN     = 1,
    PICOKNOW_KBID_TAB_GRAPHS   = 2,
    PICOKNOW_KBID_TAB_PHONES   = 3,
    PICOKNOW_KBID_TAB_POS      = 4,
    PICOKNOW_KBID_FIXED_IDS    = 7,
     
    PICOKNOW_KBID_DBG          = 8,

     
    PICOKNOW_KBID_LEX_MAIN     = 9,
    PICOKNOW_KBID_DT_POSP      = 10,
    PICOKNOW_KBID_DT_POSD      = 11,
    PICOKNOW_KBID_DT_G2P       = 12,
    PICOKNOW_KBID_FST_WPHO_1   = 13,
    PICOKNOW_KBID_FST_WPHO_2   = 14,
    PICOKNOW_KBID_FST_WPHO_3   = 15,
    PICOKNOW_KBID_FST_WPHO_4   = 16,
    PICOKNOW_KBID_FST_WPHO_5   = 17,
    PICOKNOW_KBID_DT_PHR       = 18,
    PICOKNOW_KBID_DT_ACC       = 19,
    PICOKNOW_KBID_FST_SPHO_1   = 20,
    PICOKNOW_KBID_FST_SPHO_2   = 21,
    PICOKNOW_KBID_FST_SPHO_3   = 22,
    PICOKNOW_KBID_FST_SPHO_4   = 23,
    PICOKNOW_KBID_FST_SPHO_5   = 24,

    PICOKNOW_KBID_FST_XSAMPA_PARSE   = 25,
    PICOKNOW_KBID_FST_SVOXPA_PARSE   = 26,
    PICOKNOW_KBID_FST_XSAMPA2SVOXPA   = 27,

    PICOKNOW_KBID_FST_SPHO_6   = 28,
    PICOKNOW_KBID_FST_SPHO_7   = 29,
    PICOKNOW_KBID_FST_SPHO_8   = 30,
    PICOKNOW_KBID_FST_SPHO_9   = 31,
    PICOKNOW_KBID_FST_SPHO_10   = 32,


     
    PICOKNOW_KBID_DT_DUR       = 34,
    PICOKNOW_KBID_DT_LFZ1      = 35,
    PICOKNOW_KBID_DT_LFZ2      = 36,
    PICOKNOW_KBID_DT_LFZ3      = 37,
    PICOKNOW_KBID_DT_LFZ4      = 38,
    PICOKNOW_KBID_DT_LFZ5      = 39,
    PICOKNOW_KBID_DT_MGC1      = 40,
    PICOKNOW_KBID_DT_MGC2      = 41,
    PICOKNOW_KBID_DT_MGC3      = 42,
    PICOKNOW_KBID_DT_MGC4      = 43,
    PICOKNOW_KBID_DT_MGC5      = 44,
    PICOKNOW_KBID_PDF_DUR      = 45,
    PICOKNOW_KBID_PDF_LFZ      = 46,
    PICOKNOW_KBID_PDF_MGC      = 47,
    PICOKNOW_KBID_PDF_PHS      = 48,

     
    PICOKNOW_KBID_TPP_USER_1    = 49,
    PICOKNOW_KBID_TPP_USER_2    = 50,

     
    PICOKNOW_KBID_LEX_USER_1    = 57,
    PICOKNOW_KBID_LEX_USER_2    = 58,

    PICOKNOW_KBID_DUMMY        = 127

} picoknow_kb_id_t;

#define PICOKNOW_DEFAULT_RESOURCE_NAME (picoos_char *) PICO_S(17)

#define PICOKNOW_MAX_NUM_WPHO_FSTS 5
#define PICOKNOW_MAX_NUM_SPHO_FSTS 10
#define PICOKNOW_MAX_NUM_ULEX 2
#define PICOKNOW_MAX_NUM_UTPP 2

#define PICOKNOW_KBID_WPHO_ARRAY { \
  PICOKNOW_KBID_FST_WPHO_1, \
  PICOKNOW_KBID_FST_WPHO_2, \
  PICOKNOW_KBID_FST_WPHO_3, \
  PICOKNOW_KBID_FST_WPHO_4, \
  PICOKNOW_KBID_FST_WPHO_5  \
}

#define PICOKNOW_KBID_SPHO_ARRAY { \
  PICOKNOW_KBID_FST_SPHO_1, \
  PICOKNOW_KBID_FST_SPHO_2, \
  PICOKNOW_KBID_FST_SPHO_3, \
  PICOKNOW_KBID_FST_SPHO_4, \
  PICOKNOW_KBID_FST_SPHO_5,  \
  PICOKNOW_KBID_FST_SPHO_6,  \
  PICOKNOW_KBID_FST_SPHO_7,  \
  PICOKNOW_KBID_FST_SPHO_8,  \
  PICOKNOW_KBID_FST_SPHO_9,  \
  PICOKNOW_KBID_FST_SPHO_10  \
}

#define PICOKNOW_KBID_ULEX_ARRAY { \
    PICOKNOW_KBID_LEX_USER_1, \
    PICOKNOW_KBID_LEX_USER_2, \
}

#define PICOKNOW_KBID_UTPP_ARRAY { \
    PICOKNOW_KBID_TPP_USER_1, \
    PICOKNOW_KBID_TPP_USER_2, \
}

 
#define PICOKNOW_MAX_KB_NAME_SIZ 16

 
#define PICOKNOW_MAX_NUM_RESOURCE_KBS 64






typedef struct picoknow_knowledge_base * picoknow_KnowledgeBase;

typedef pico_status_t (* picoknow_kbSubDeallocate) (register picoknow_KnowledgeBase thiz, picoos_MemoryManager mm);

typedef struct picoknow_knowledge_base {
     
    picoknow_KnowledgeBase next;
    picoknow_kb_id_t id;
    picoos_uint8 * base;  
    picoos_uint32 size;  

     
    picoknow_kbSubDeallocate subDeallocate;
    void * subObj;
} picoknow_knowledge_base_t;

extern picoknow_KnowledgeBase picoknow_newKnowledgeBase(picoos_MemoryManager mm);

extern void picoknow_disposeKnowledgeBase(picoos_MemoryManager mm, picoknow_KnowledgeBase * thiz);




#endif  
