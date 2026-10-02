
























#ifndef PICODATA_H_
#define PICODATA_H_

#include "picodefs.h"
#include "picoos.h"
#include "picotrns.h"
#include "picokfst.h"
#include "picorsrc.h"









#define PICODATA_MAX_ITEMS_PER_PHRASE 30




















 
#define PICODATA_ITEMIND_TYPE  0
#define PICODATA_ITEMIND_INFO1 1
#define PICODATA_ITEMIND_INFO2 2
#define PICODATA_ITEMIND_LEN   3




typedef struct picodata_char_buffer * picodata_CharBuffer;

picodata_CharBuffer picodata_newCharBuffer(picoos_MemoryManager mm,
        picoos_Common common, picoos_objsize_t size);

void picodata_disposeCharBuffer(picoos_MemoryManager mm,
                                picodata_CharBuffer * thiz);

 
pico_status_t picodata_cbPutCh(register picodata_CharBuffer thiz, picoos_char ch);

 
picoos_int16 picodata_cbGetCh(register picodata_CharBuffer thiz);

 
pico_status_t picodata_cbReset (register picodata_CharBuffer thiz);

 





 
#define PICODATA_ITEM_HEADSIZE 4

typedef struct picodata_itemhead
{
    picoos_uint8 type;
    picoos_uint8 info1;
    picoos_uint8 info2;
    picoos_uint8 len;
} picodata_itemhead_t;


 
 
#define PICODATA_ACC0  '\x30'  
#define PICODATA_ACC1  '\x31'  
#define PICODATA_ACC2  '\x32'  
#define PICODATA_ACC3  '\x33'  
#define PICODATA_ACC4  '\x34'  






#define PICODATA_POS_XNPR 20
#define PICODATA_POS_XN   21
#define PICODATA_POS_XV   22
#define PICODATA_POS_XA   23
#define PICODATA_POS_XADV 24
#define PICODATA_POS_XX   25

 
 
 
#define PICODATA_ITEM_WSEQ_GRAPH '\x73'   
#define PICODATA_ITEM_TOKEN      '\x74'   
#define PICODATA_ITEM_WORDGRAPH  '\x67'   
#define PICODATA_ITEM_WORDINDEX  '\x69'   
#define PICODATA_ITEM_WORDPHON   '\x77'   
#define PICODATA_ITEM_SYLLPHON   '\x79'   
#define PICODATA_ITEM_BOUND      '\x62'   
     
#define PICODATA_ITEM_PUNC       '\x70'   
#define PICODATA_ITEM_CMD        '\x63'   
#define PICODATA_ITEM_PHONE      '\x68'     
#define PICODATA_ITEM_FRAME_PAR  '\x6b'     
#define PICODATA_ITEM_FRAME      '\x66'     
#define PICODATA_ITEM_OTHER      '\x6f'   
#define PICODATA_ITEM_ERR        '\x00'   

 
#define PICODATA_ITEMINFO1_ERR   '\x00'      
#define PICODATA_ITEMINFO1_NA    '\x01'      

 
#define PICODATA_ITEMINFO2_ERR   '\x00'     
#define PICODATA_ITEMINFO2_NA    '\x01'     

 
 
#define PICODATA_ITEMINFO1_PUNC_SENTEND       '\x73'   
#define PICODATA_ITEMINFO1_PUNC_PHRASEEND     '\x70'   
#define PICODATA_ITEMINFO1_PUNC_FLUSH         '\x66'   
 
#define PICODATA_ITEMINFO2_PUNC_SENT_T        '\x74'   
#define PICODATA_ITEMINFO2_PUNC_SENT_Q        '\x71'   
#define PICODATA_ITEMINFO2_PUNC_SENT_E        '\x65'   
#define PICODATA_ITEMINFO2_PUNC_PHRASE        '\x70'   
#define PICODATA_ITEMINFO2_PUNC_PHRASE_FORCED '\x66'   
 
 
 
#define PICODATA_ITEMINFO1_BOUND_SBEG  '\x62'   
#define PICODATA_ITEMINFO1_BOUND_SEND  '\x73'   
#define PICODATA_ITEMINFO1_BOUND_TERM  '\x74'   
#define PICODATA_ITEMINFO1_BOUND_PHR0  '\x30'   
#define PICODATA_ITEMINFO1_BOUND_PHR1  '\x31'   
#define PICODATA_ITEMINFO1_BOUND_PHR2  '\x32'   
#define PICODATA_ITEMINFO1_BOUND_PHR3  '\x33'   
 
#define PICODATA_ITEMINFO2_BOUNDTYPE_P '\x50'   
#define PICODATA_ITEMINFO2_BOUNDTYPE_T '\x54'   
#define PICODATA_ITEMINFO2_BOUNDTYPE_Q '\x51'   
#define PICODATA_ITEMINFO2_BOUNDTYPE_E '\x45'   
 
 
 
#define PICODATA_ITEMINFO1_CMD_FLUSH          'f'  
#define PICODATA_ITEMINFO1_CMD_PLAY           'p'  
#define PICODATA_ITEMINFO1_CMD_SAVE           's'  
#define PICODATA_ITEMINFO1_CMD_UNSAVE         'u'  
#define PICODATA_ITEMINFO1_CMD_PROSDOMAIN     'd'  
#define PICODATA_ITEMINFO1_CMD_SPELL          'e' /* 101 spell command : info 2 contains start/stop info,
                                                    spell type/pause len as little endian uint16 in item content */
#define PICODATA_ITEMINFO1_CMD_IGNSIG         'i'  
#define PICODATA_ITEMINFO1_CMD_PHONEME        'o'  
#define PICODATA_ITEMINFO1_CMD_IGNORE         'I'  
#define PICODATA_ITEMINFO1_CMD_SIL            'z' /* silence command : info 2 contains type of silence;
                                                     silence duration as little endian uint16 in item content */
#define PICODATA_ITEMINFO1_CMD_CONTEXT        'c'  
#define PICODATA_ITEMINFO1_CMD_VOICE          'v'  
#define PICODATA_ITEMINFO1_CMD_MARKER         'm'  
#define PICODATA_ITEMINFO1_CMD_PITCH          'P' /* 80 pitch command : abs/rel info in info 2; pitch level as little endian
                                                     uint16 in item content; relative value is in promille */
#define PICODATA_ITEMINFO1_CMD_SPEED          'R' /* 82 speed command : abs/rel info in info 2, speed level as little endian
                                                     uint16 in item content; elative value is in promille */
#define PICODATA_ITEMINFO1_CMD_VOLUME         'V' /* 86 volume command : abs/rel info in info 2, volume level as little endian
                                                     uint16 in item content; relative value is in promille */
#define PICODATA_ITEMINFO1_CMD_SPEAKER        'S' /* 83 speaker command : abs/rel info in info 2, speaker level as little endian
                                                     uint16 in item content; relative value is in promille */

 
#define PICODATA_ITEMINFO2_CMD_TO_TOK  't'   
#define PICODATA_ITEMINFO2_CMD_TO_PR   'g'   
#define PICODATA_ITEMINFO2_CMD_TO_WA   'w'   
#define PICODATA_ITEMINFO2_CMD_TO_SA   'a'   
#define PICODATA_ITEMINFO2_CMD_TO_ACPH 'h'   
#define PICODATA_ITEMINFO2_CMD_TO_SPHO 'p'   
#define PICODATA_ITEMINFO2_CMD_TO_PAM  'q'   
#define PICODATA_ITEMINFO2_CMD_TO_CEP  'c'   
#define PICODATA_ITEMINFO2_CMD_TO_SIG  's'   



#define PICODATA_ITEMINFO2_CMD_TO_UNKNOWN 255

 
#define PICODATA_ITEMINFO2_CMD_START  's'
#define PICODATA_ITEMINFO2_CMD_END    'e'

 
#define PICODATA_ITEMINFO2_CMD_ABSOLUTE 'a'
#define PICODATA_ITEMINFO2_CMD_RELATIVE 'r'

 
 
 
#define PICODATA_ITEMINFO1_TOKTYPE_SPACE     'W'
#define PICODATA_ITEMINFO1_TOKTYPE_LETTERV   'V'
#define PICODATA_ITEMINFO1_TOKTYPE_LETTER    'L'
#define PICODATA_ITEMINFO1_TOKTYPE_DIGIT     'D'
#define PICODATA_ITEMINFO1_TOKTYPE_SEQ       'S'
#define PICODATA_ITEMINFO1_TOKTYPE_CHAR      'C'
#define PICODATA_ITEMINFO1_TOKTYPE_BEGIN     'B'
#define PICODATA_ITEMINFO1_TOKTYPE_END       'E'
#define PICODATA_ITEMINFO1_TOKTYPE_UNDEFINED 'U'
 
 



































#define PICODATA_ITEMINFO1_FRAME_PAR_DATA_FORMAT_FIXED  '\x78'  
#define PICODATA_ITEMINFO1_FRAME_PAR_DATA_FORMAT_FLOAT  '\x66'  













pico_status_t picodata_cbGetItem(register picodata_CharBuffer thiz,
        picoos_uint8 *buf, const picoos_uint16 blenmax,
        picoos_uint16 *blen);









pico_status_t picodata_cbGetSpeechData(register picodata_CharBuffer thiz,
        picoos_uint8 *buf, const picoos_uint16 blenmax,
        picoos_uint16 *blen);








pico_status_t picodata_cbPutItem(register picodata_CharBuffer thiz,
        const picoos_uint8 *buf, const picoos_uint16 blenmax,
        picoos_uint16 *blen);

 
picoos_uint8 picodata_cbGetFrontItemType(register picodata_CharBuffer thiz);





 
picoos_uint8 is_valid_itemtype(const picoos_uint8 ch);









pico_status_t picodata_get_itemparts_nowarn(
        const picoos_uint8 *buf, const picoos_uint16 blenmax,
        picodata_itemhead_t *head, picoos_uint8 *content,
        const picoos_uint16 clenmax, picoos_uint16 *clen);









pico_status_t picodata_get_itemparts(
        const picoos_uint8 *buf, const picoos_uint16 blenmax,
        picodata_itemhead_t *head, picoos_uint8 *content,
        const picoos_uint16 clenmax, picoos_uint16 *clen);









pico_status_t picodata_put_itemparts(const picodata_itemhead_t *head,
        const picoos_uint8 *content, const picoos_uint16 clenmax,
        picoos_uint8 *buf, const picoos_uint16 blenmax, picoos_uint16 *blen);








pico_status_t picodata_get_iteminfo(
        picoos_uint8 *buf, const picoos_uint16 blenmax,
        picodata_itemhead_t *head, picoos_uint8 **content);










pico_status_t picodata_copy_item(const picoos_uint8 *inbuf,
        const picoos_uint16 inlenmax, picoos_uint8 *outbuf,
        const picoos_uint16 outlenmax, picoos_uint16 *numb);






pico_status_t picodata_set_iteminfo1(picoos_uint8 *buf,
        const picoos_uint16 blenmax, const picoos_uint8 info);






pico_status_t picodata_set_iteminfo2(picoos_uint8 *buf,
        const picoos_uint16 blenmax, const picoos_uint8 info);






pico_status_t picodata_set_itemlen(picoos_uint8 *buf,
        const picoos_uint16 blenmax, const picoos_uint8 len);




picoos_uint8 picodata_is_valid_item(const picoos_uint8 *item,
        const picoos_uint16 ilenmax);

 
picoos_uint8 picodata_is_valid_itemhead(const picodata_itemhead_t *head);





 

#define PICODATA_MAX_ITEMSIZE (picoos_uint16) (PICODATA_ITEM_HEADSIZE + 256)

 
#define PICODATA_BUFSIZE_DEFAULT (picoos_uint16) PICODATA_MAX_ITEMSIZE
#define PICODATA_BUFSIZE_TEXT    (picoos_uint16)  1 * PICODATA_BUFSIZE_DEFAULT
#define PICODATA_BUFSIZE_TOK     (picoos_uint16)  2 * PICODATA_BUFSIZE_DEFAULT
#define PICODATA_BUFSIZE_PR      (picoos_uint16)  2 * PICODATA_BUFSIZE_DEFAULT
#define PICODATA_BUFSIZE_WA      (picoos_uint16)  2 * PICODATA_BUFSIZE_DEFAULT
#define PICODATA_BUFSIZE_SA      (picoos_uint16)  2 * PICODATA_BUFSIZE_DEFAULT
#define PICODATA_BUFSIZE_ACPH    (picoos_uint16)  2 * PICODATA_BUFSIZE_DEFAULT
#define PICODATA_BUFSIZE_SPHO    (picoos_uint16)  4 * PICODATA_BUFSIZE_DEFAULT
#define PICODATA_BUFSIZE_PAM     (picoos_uint16)  4 * PICODATA_BUFSIZE_DEFAULT
#define PICODATA_BUFSIZE_CEP     (picoos_uint16) 16 * PICODATA_BUFSIZE_DEFAULT
#define PICODATA_BUFSIZE_SIG     (picoos_uint16) 16 * PICODATA_BUFSIZE_DEFAULT
#define PICODATA_BUFSIZE_SINK     (picoos_uint16) 1 * PICODATA_BUFSIZE_DEFAULT

 
typedef enum picodata_putype {
    PICODATA_PUTYPE_TEXT,    
    PICODATA_PUTYPE_TOK,     
    PICODATA_PUTYPE_PR,      
    PICODATA_PUTYPE_WA,      
    PICODATA_PUTYPE_SA,      
    PICODATA_PUTYPE_ACPH,      
    PICODATA_PUTYPE_SPHO,    
    PICODATA_PUTYPE_PAM,     
    PICODATA_PUTYPE_CEP,     
    PICODATA_PUTYPE_SIG,      
    PICODATA_PUTYPE_SINK      
} picodata_putype_t;

picoos_uint16 picodata_get_default_buf_size (picodata_putype_t puType);

 
typedef enum picodata_step_result {
    PICODATA_PU_ERROR,
      
    PICODATA_PU_IDLE,  
    PICODATA_PU_BUSY,  
    PICODATA_PU_ATOMIC,  
    PICODATA_PU_OUT_FULL  
} picodata_step_result_t;

typedef struct picodata_processing_unit * picodata_ProcessingUnit;

picodata_ProcessingUnit picodata_newProcessingUnit(
        picoos_MemoryManager mm,
        picoos_Common common,
        picodata_CharBuffer cbIn,
        picodata_CharBuffer cbOut,
        picorsrc_Voice voice);

void picodata_disposeProcessingUnit(
        picoos_MemoryManager mm,
        picodata_ProcessingUnit * thiz);

picodata_CharBuffer picodata_getCbIn(picodata_ProcessingUnit thiz);
picodata_CharBuffer picodata_getCbOut(picodata_ProcessingUnit thiz);
pico_status_t picodata_setCbIn(picodata_ProcessingUnit thiz, picodata_CharBuffer cbIn);
pico_status_t picodata_setCbOut(picodata_ProcessingUnit thiz, picodata_CharBuffer cbOut);

 
typedef pico_status_t (* picodata_puInitializeMethod) (register picodata_ProcessingUnit thiz, picoos_int32 mode);
typedef pico_status_t (* picodata_puTerminateMethod) (register picodata_ProcessingUnit thiz);
typedef picodata_step_result_t (* picodata_puStepMethod) (register picodata_ProcessingUnit thiz, picoos_int16 mode, picoos_uint16 * numBytesOutput);
typedef pico_status_t (* picodata_puSubDeallocateMethod) (register picodata_ProcessingUnit thiz, picoos_MemoryManager mm);

typedef struct picodata_processing_unit
{
     
    picodata_puInitializeMethod initialize;
    picodata_puStepMethod       step;
    picodata_puTerminateMethod  terminate;
    picorsrc_Voice              voice;

     
    picoos_Common                  common;
    picodata_CharBuffer            cbIn, cbOut;
    picodata_puSubDeallocateMethod subDeallocate;
    void * subObj;

} picodata_processing_unit_t;

 
#define PICODATA_PUTYPE_TEXT_OUTPUT_EXTENSION   (picoos_uchar*)PICO_S(0)
#define PICODATA_PUTYPE_TOK_INPUT_EXTENSION     PICODATA_PUTYPE_TEXT_OUTPUT_EXTENSION
#define PICODATA_PUTYPE_TOK_OUTPUT_EXTENSION    (picoos_uchar*)PICO_S(1)
#define PICODATA_PUTYPE_PR_INPUT_EXTENSION      PICODATA_PUTYPE_TOK_OUTPUT_EXTENSION
#define PICODATA_PUTYPE_PR_OUTPUT_EXTENSION     (picoos_uchar*)PICO_S(2)
#define PICODATA_PUTYPE_WA_INPUT_EXTENSION      PICODATA_PUTYPE_PR_OUTPUT_EXTENSION
#define PICODATA_PUTYPE_WA_OUTPUT_EXTENSION     (picoos_uchar*)PICO_S(3)
#define PICODATA_PUTYPE_SA_INPUT_EXTENSION      PICODATA_PUTYPE_WA_OUTPUT_EXTENSION
#define PICODATA_PUTYPE_SA_OUTPUT_EXTENSION     (picoos_uchar*)PICO_S(4)
#define PICODATA_PUTYPE_ACPH_INPUT_EXTENSION    PICODATA_PUTYPE_SA_OUTPUT_EXTENSION
#define PICODATA_PUTYPE_ACPH_OUTPUT_EXTENSION   (picoos_uchar*)PICO_S(5)
#define PICODATA_PUTYPE_SPHO_INPUT_EXTENSION    PICODATA_PUTYPE_ACPH_OUTPUT_EXTENSION
#define PICODATA_PUTYPE_SPHO_OUTPUT_EXTENSION   (picoos_uchar*)PICO_S(6)
#define PICODATA_PUTYPE_PAM_INPUT_EXTENSION     PICODATA_PUTYPE_SPHO_OUTPUT_EXTENSION
#define PICODATA_PUTYPE_PAM_OUTPUT_EXTENSION    (picoos_uchar*)PICO_S(7)
#define PICODATA_PUTYPE_CEP_INPUT_EXTENSION     PICODATA_PUTYPE_PAM_OUTPUT_EXTENSION
#define PICODATA_PUTYPE_CEP_OUTPUT_EXTENSION    (picoos_uchar*)PICO_S(8)
#define PICODATA_PUTYPE_SIG_INPUT_EXTENSION     PICODATA_PUTYPE_CEP_OUTPUT_EXTENSION    
#define PICODATA_PUTYPE_SIG_OUTPUT_EXTENSION    (picoos_uchar*)PICO_S(9)
#define PICODATA_PUTYPE_SINK_INPUT_EXTENSION    PICODATA_PUTYPE_SIG_OUTPUT_EXTENSION

 
#define PICODATA_PUTYPE_WAV_INPUT_EXTENSION    (picoos_uchar*)PICO_S(10)

 
#define PICODATA_PUTYPE_WAV_OUTPUT_EXTENSION    (picoos_uchar*)PICO_S(10)





picoos_uint8 picodata_getPuTypeFromExtension(picoos_uchar * filename, picoos_bool input);

#define PICODATA_XSAMPA (picoos_uchar *)PICO_S(11)
#define PICODATA_SAMPA (picoos_uchar *)PICO_S(12)
#define PICODATA_SVOXPA (picoos_uchar *)PICO_S(13)

 












 
pico_status_t picodata_mapPAStrToPAIds(
        picotrns_SimpleTransducer transducer,
        picoos_Common common,
        picokfst_FST xsampa_parser,
        picokfst_FST svoxpa_parser,
        picokfst_FST xsampa2svoxpa_mapper,
        picoos_uchar * inputPhones,
        picoos_uchar * alphabet,
        picoos_uint8 * outputPhoneIds,
        picoos_int32 maxOutputPhoneIds);

 
#define PICODATA_PRECISION 10
 
#define PICODATA_PREC_HALF 512

void picodata_transformDurations(
        picoos_uint8 frame_duration_exp,
        picoos_int8 array_length,
        picoos_uint8 * inout,
        const picoos_uint16 * weight,   
        picoos_int16 mintarget,  
        picoos_int16 maxtarget,  
        picoos_int16 facttarget, 



        picoos_int16 * dur_rest  
        );







#if defined (PICO_DEBUG)



picoos_char * picodata_head_to_string(const picodata_itemhead_t *head,
                                      picoos_char * str, picoos_uint16 strsize);




void picodata_info_item(const picoknow_KnowledgeBase kb,
                        const picoos_uint8 *pref6ch,
                        const picoos_uint8 *item,
                        const picoos_uint16 itemlenmax,
                        const picoos_char *filterfn);


#define PICODATA_INFO_ITEM(kb, pref, item, itemlenmax)   \
    PICODBG_INFO_CTX(); \
    picodata_info_item(kb, pref, item, itemlenmax, (picoos_char *)__FILE__)



#else

#define PICODATA_INFO_ITEM(kb, pref, item, itemlenmax)

#endif



#endif  
