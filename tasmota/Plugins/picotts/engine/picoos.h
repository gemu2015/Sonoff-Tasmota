































#ifndef PICOOS_H_
#define PICOOS_H_

#include "picodefs.h"
#include "picopal.h"





 
#define PICOOS_DIV_USE_INV PICOPAL_DIV_USE_INV

 
 
 

typedef picopal_uint8   picoos_uint8;
typedef picopal_uint16  picoos_uint16;
typedef picopal_uint32  picoos_uint32;

typedef picopal_int8    picoos_int8;
typedef picopal_int16   picoos_int16;
typedef picopal_int32   picoos_int32;

typedef picopal_double  picoos_double;
typedef picopal_single  picoos_single;

typedef picopal_char    picoos_char;
typedef picopal_uchar   picoos_uchar;

typedef picopal_uint8   picoos_bool;

typedef picopal_objsize_t picoos_objsize_t;
typedef picopal_ptrdiff_t picoos_ptrdiff_t;

 
 
 


picoos_int32 picoos_atoi(const picoos_char *);
picoos_int8 picoos_strcmp(const picoos_char *, const picoos_char *);
picoos_int8 picoos_strncmp(const picoos_char *a, const picoos_char *b, picoos_objsize_t siz);
picoos_uint32 picoos_strlen(const picoos_char *);
picoos_char * picoos_strchr(const picoos_char *, picoos_char);
picoos_char *picoos_strstr(const picoos_char *s, const picoos_char *substr);
picoos_int16 picoos_slprintf(picoos_char * b, picoos_uint32 bsize, const picoos_char *f, ...);
picoos_char * picoos_strcpy(picoos_char *, const picoos_char *);
picoos_char * picoos_strcat(picoos_char *, const picoos_char *);

 
void * picoos_mem_copy(const void * src, void * dst,  picoos_objsize_t length);

 
picoos_objsize_t picoos_strlcpy(picoos_char *dst, const picoos_char *src, picoos_objsize_t siz);
void * picoos_mem_set(void * dest, picoos_uint8 byte_val, picoos_objsize_t length);

picoos_double picoos_cos(const picoos_double cos_arg);
picoos_double picoos_sin(const picoos_double sin_arg);
picoos_double picoos_fabs(const picoos_double fabs_arg);

picoos_double picoos_quick_exp(const picoos_double y);


void picoos_get_sep_part_str (picoos_char string[], picoos_int32 stringlen, picoos_int32 * ind, picoos_char sepCh, picoos_char part[], picoos_int32 maxsize, picoos_uint8 * done);
pico_status_t picoos_string_to_uint32 (picoos_char str[], picoos_uint32 * res);
pico_status_t picoos_string_to_int32 (picoos_char str[], picoos_int32 * res);

 
 
 



typedef struct memory_manager * picoos_MemoryManager;
typedef struct picoos_exception_manager * picoos_ExceptionManager;
typedef struct picoos_file * picoos_File;






typedef struct picoos_common * picoos_Common;

 
typedef struct picoos_common {
    picoos_ExceptionManager em;
    picoos_MemoryManager mm;
    picoos_File fileList;
} picoos_common_t;

picoos_Common picoos_newCommon(picoos_MemoryManager mm);

void picoos_disposeCommon(picoos_MemoryManager mm, picoos_Common * thiz);


 
 
 

typedef picoos_char * byte_ptr_t;

#define PICOOS_ALIGN_SIZE 8



void * picoos_raw_malloc(byte_ptr_t raw_mem,
        picoos_objsize_t raw_mem_size, picoos_objsize_t alloc_size,
        byte_ptr_t * rest_mem, picoos_objsize_t * rest_mem_size);






picoos_MemoryManager picoos_newMemoryManager(
        void *raw_memory,
        picoos_objsize_t size,
        picoos_bool enableMemProt);



void picoos_disposeMemoryManager(picoos_MemoryManager * mm);


void * picoos_allocate(picoos_MemoryManager thiz, picoos_objsize_t byteSize);
void picoos_deallocate(picoos_MemoryManager thiz, void * * adr);








void *picoos_allocProtMem(picoos_MemoryManager mm, picoos_objsize_t byteSize);




void picoos_deallocProtMem(picoos_MemoryManager mm, void **addr);






void picoos_protectMem(
        picoos_MemoryManager mm,
        void *addr,
        picoos_objsize_t len,
        picoos_bool enable);

void picoos_getMemUsage(
        picoos_MemoryManager thiz,
        picoos_bool resetIncremental,
        picoos_int32 *usedBytes,
        picoos_int32 *incrUsedBytes,
        picoos_int32 *maxUsedBytes);

void picoos_showMemUsage(
        picoos_MemoryManager thiz,
        picoos_bool incremental,
        picoos_bool resetIncremental);

 
 
 






#define PICOOS_MAX_EXC_MSG_LEN 512
#define PICOOS_MAX_WARN_MSG_LEN 64
#define PICOOS_MAX_NUM_WARNINGS 8

void picoos_setErrorMsg(picoos_char * dst, picoos_objsize_t siz,
        picoos_int16 code, picoos_char * base, const picoos_char *fmt, ...);


picoos_ExceptionManager picoos_newExceptionManager(picoos_MemoryManager mm);

void picoos_disposeExceptionManager(picoos_MemoryManager mm,
        picoos_ExceptionManager * thiz);


void picoos_emReset(picoos_ExceptionManager thiz);





pico_status_t picoos_emRaiseException(picoos_ExceptionManager thiz,
        pico_status_t exceptionCode, picoos_char * baseMessage, picoos_char * fmt, ...);

pico_status_t picoos_emGetExceptionCode(picoos_ExceptionManager thiz);

void picoos_emGetExceptionMessage(picoos_ExceptionManager thiz, picoos_char * msg, picoos_uint16 maxsize);

void picoos_emRaiseWarning(picoos_ExceptionManager thiz,
        pico_status_t warningCode, picoos_char * baseMessage, picoos_char * fmt, ...);

picoos_uint8 picoos_emGetNumOfWarnings(picoos_ExceptionManager thiz);

pico_status_t picoos_emGetWarningCode(picoos_ExceptionManager thiz, picoos_uint8 warnNum);

void picoos_emGetWarningMessage(picoos_ExceptionManager thiz, picoos_uint8 warnNum, picoos_char * msg, picoos_uint16 maxsize);




 
 
 

#define picoos_MaxFileNameLen 512
#define picoos_MaxKeyLen 512
#define picoos_MaxPathLen 512
#define picoos_MaxPathListLen 2048

typedef picoos_char picoos_Key[picoos_MaxKeyLen];
typedef picoos_char picoos_FileName[picoos_MaxFileNameLen];
typedef picoos_char picoos_Path[picoos_MaxPathLen];
typedef picoos_char picoos_PathList[picoos_MaxPathListLen];


 




 
picoos_uint8 picoos_OpenBinary(picoos_Common g, picoos_File * f, picoos_char name[]);


 
picoos_uint8  picoos_ReadByte(picoos_File f, picoos_uint8 * by);





picoos_uint8  picoos_ReadBytes(picoos_File f, picoos_uint8 bytes[],
        picoos_uint32 * len);




picoos_uint8 picoos_CreateBinary(picoos_Common g, picoos_File * f, picoos_char name[]);

picoos_uint8  picoos_WriteByte(picoos_File f, picoos_char by);



picoos_uint8  picoos_WriteBytes(picoos_File f, const picoos_char bytes[],
        picoos_int32 * len);


 
picoos_uint8 picoos_CloseBinary(picoos_Common g, picoos_File * f);






pico_status_t picoos_read_le_int16 (picoos_File file, picoos_int16 * val);
pico_status_t picoos_read_le_uint16 (picoos_File file, picoos_uint16 * val);
pico_status_t picoos_read_le_uint32 (picoos_File file, picoos_uint32 * val);


pico_status_t picoos_read_pi_uint16 (picoos_File file, picoos_uint16 * val);
pico_status_t picoos_read_pi_uint32 (picoos_File file, picoos_uint32 * val);

pico_status_t picoos_write_le_uint16 (picoos_File file, picoos_uint16 val);
pico_status_t picoos_write_le_uint32 (picoos_File file, picoos_uint32 val);








 
 



picoos_bool picoos_Eof(picoos_File f);




picoos_bool  picoos_SetPos(picoos_File f, picoos_int32 pos);

 
picoos_bool picoos_GetPos(picoos_File f, picoos_uint32 * pos);

 
picoos_bool picoos_FileLength(picoos_File f, picoos_uint32 * len);

 
picoos_bool picoos_Name(picoos_File f, picoos_char name[], picoos_uint32 maxsize);

 
picoos_bool picoos_FileExists(picoos_Common g, picoos_char name[]  );

 
picoos_bool  picoos_Delete(picoos_char name[]);

 
picoos_bool  picoos_Rename(picoos_char oldName[], picoos_char newName[]);


 
 
 

#define SAMPLE_FREQ_16KHZ (picoos_uint32) 16000

typedef enum {
    FILE_TYPE_WAV,
    FILE_TYPE_AU,
    FILE_TYPE_RAW,
    FILE_TYPE_OTHER
} wave_file_type_t;

typedef enum {
    FORMAT_TAG_LIN = 1,  
    FORMAT_TAG_ALAW = 6,  
    FORMAT_TAG_ULAW = 7  
     
} wave_format_tag_t;


typedef enum {
     
    PICOOS_ENC_LIN = FORMAT_TAG_LIN,   
    PICOOS_ENC_ALAW = FORMAT_TAG_ALAW,  
    PICOOS_ENC_ULAW = FORMAT_TAG_ULAW,  
     
    PICOOS_ENC_OTHER = 5000   
    }  picoos_encoding_t;

typedef struct picoos_sd_file * picoos_SDFile;

 
























extern picoos_bool picoos_sdfOpenIn (picoos_Common g, picoos_SDFile * sdFile, picoos_char fileName[], picoos_uint32 * sf, picoos_encoding_t * enc, picoos_uint32 * nrSamples);


extern picoos_bool picoos_sdfGetSamples (picoos_SDFile sdFile, picoos_uint32 start, picoos_uint32 * nrSamples, picoos_int16 samples[]);


extern picoos_bool picoos_sdfCloseIn (picoos_Common g, picoos_SDFile * sdFile);


 

extern picoos_bool picoos_sdfOpenOut (picoos_Common g, picoos_SDFile * sdFile, picoos_char fileName[], int sf, picoos_encoding_t enc);


extern picoos_bool picoos_sdfPutSamples (picoos_SDFile sdFile, picoos_uint32 nrSamples, picoos_int16 samples[]);











extern picoos_bool picoos_sdfCloseOut (picoos_Common g, picoos_SDFile * sdFile);


 
 
 

#define PICOOS_MAX_FIELD_STRING_LEN 32  

#define PICOOS_MAX_NUM_HEADER_FIELDS 10
#define PICOOS_NUM_BASIC_HEADER_FIELDS 5

#define PICOOS_HEADER_NAME 0
#define PICOOS_HEADER_VERSION 1
#define PICOOS_HEADER_DATE 2
#define PICOOS_HEADER_TIME 3
#define PICOOS_HEADER_CONTENT_TYPE 4

#define PICOOS_MAX_HEADER_STRING_LEN (PICOOS_MAX_NUM_HEADER_FIELDS * (2 * PICOOS_MAX_FIELD_STRING_LEN))

typedef picoos_char picoos_field_string_t[PICOOS_MAX_FIELD_STRING_LEN];

typedef picoos_char picoos_header_string_t[PICOOS_MAX_HEADER_STRING_LEN];

typedef enum {PICOOS_FIELD_IGNORE, PICOOS_FIELD_EQUAL, PICOOS_FIELD_COMPAT} picoos_compare_op_t;

 
typedef struct picoos_file_header_field {
    picoos_field_string_t key;
    picoos_field_string_t value;
    picoos_compare_op_t op;
} picoos_file_header_field_t;

 
typedef struct picoos_file_header * picoos_FileHeader;
typedef struct picoos_file_header {
    picoos_uint8 numFields;
    picoos_file_header_field_t  field[PICOOS_MAX_NUM_HEADER_FIELDS];
} picoos_file_header_t;


pico_status_t picoos_clearHeader(picoos_FileHeader header);

pico_status_t picoos_setHeaderField(picoos_FileHeader header, picoos_uint8 index, picoos_char * key, picoos_char * value, picoos_compare_op_t op);

 
pico_status_t picoos_getHeaderField(picoos_FileHeader header, picoos_uint8 index, picoos_field_string_t key, picoos_field_string_t value, picoos_compare_op_t * op);

 




pico_status_t picoos_hdrParseHeader(picoos_FileHeader header, picoos_header_string_t str);

pico_status_t picoos_getSVOXHeaderString(picoos_char * str, picoos_uint8 * len, picoos_uint32 maxlen);

pico_status_t picoos_readPicoHeader(picoos_File f, picoos_uint32 * headerlen);



 
 
 


picoos_uint8 picoos_has_extension(const picoos_char *str, const picoos_char *suf);

 
 
 

pico_status_t picoos_string_to_int32(picoos_char str[],
        picoos_int32 * res);

pico_status_t picoos_string_to_uint32(picoos_char str[],
        picoos_uint32 * res);



void picoos_get_sep_part_str(picoos_char string[],
        picoos_int32 stringlen, picoos_int32 * ind, picoos_char sepCh,
        picoos_char part[], picoos_int32 maxsize, picoos_uint8 * done);







picoos_uint8 picoos_get_str (picoos_char * fromStr, picoos_uint32 * pos, picoos_char * toStr, picoos_objsize_t maxsize);


pico_status_t picoos_read_mem_pi_uint16 (picoos_uint8 * data, picoos_uint32 * pos, picoos_uint16 * val);

pico_status_t picoos_read_mem_pi_uint32 (picoos_uint8 * data, picoos_uint32 * pos, picoos_uint32 * val);

pico_status_t picoos_write_mem_pi_uint16 (picoos_uint8 * data, picoos_uint32 * pos, picoos_uint16 val);


 
 
 

void picoos_get_timer(picopal_uint32 * sec, picopal_uint32 * usec);




#endif  
