
































#ifndef PICOPAL_H_
#define PICOPAL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <stddef.h>
#include "picopltf.h"
#include "picodefs.h"





 
 
 

#define TRUE 1
#define FALSE 0

#ifndef NULL
#define NULL 0
#endif

#define NULLC '\000'


 
#define PICOPAL_DIV_USE_INV 0


 
 
#if defined(PICO_DEBUG)
    extern int numlongmult, numshortmult;
#endif


typedef signed int pico_status_t;


 



#define PICO_EOF                        (pico_status_t)    -1


 
 
 


   
#define PICOPAL_OS_NIL        0   
#define PICOPAL_OS_WINDOWS    1
 
#define PICOPAL_OS_GENERIC   99  

 
 
 

typedef unsigned char   picopal_uint8;
typedef unsigned short  picopal_uint16;
typedef unsigned int    picopal_uint32;

typedef signed char     picopal_int8;
typedef signed short    picopal_int16;
typedef signed int      picopal_int32;

typedef float           picopal_single;
typedef float           picopal_double;

typedef unsigned char     picopal_char;

typedef unsigned char   picopal_uchar;

typedef size_t    picopal_objsize_t;
typedef ptrdiff_t picopal_ptrdiff_t;

 
 
 

picopal_int32 picopal_atoi(const picopal_char *);

picopal_int32 picopal_strcmp(const picopal_char *, const picopal_char *);
picopal_int32 picopal_strncmp(const picopal_char *a, const picopal_char *b, picopal_objsize_t siz);
picopal_objsize_t picopal_strlen(const picopal_char *);
picopal_char * picopal_strchr(const picopal_char *, picopal_char);
picopal_char * picopal_strcpy(picopal_char *d, const picopal_char *s);
picopal_char *picopal_strstr(const picopal_char *s, const picopal_char *substr);
picopal_char *picopal_strcat(picopal_char *dest, const picopal_char *src);
picopal_int16 picopal_sprintf(picopal_char * dst, const picopal_char *fmt, ...);

 
void * picopal_mem_copy(const void * src, void * dst,  picopal_objsize_t length);

 
void * picopal_mem_set(void * dest, picopal_uint8 byte_val, picopal_objsize_t length);

 
picopal_objsize_t picopal_vslprintf(picopal_char * dst, picopal_objsize_t siz, const picopal_char *fmt, va_list args);
picopal_objsize_t picopal_slprintf(picopal_char * dst, picopal_objsize_t siz, const picopal_char *fmt,   ...);
picopal_objsize_t picopal_strlcpy(picopal_char *dst, const picopal_char *src, picopal_objsize_t siz);

 








 
 
 

picopal_double picopal_cos (const picopal_double cos_arg);
picopal_double picopal_sin (const picopal_double sin_arg);
picopal_double picopal_fabs (const picopal_double fabs_arg);




 
 
 

extern picopal_char picopal_eol(void);

#define picopal_FILE      FILE


 
#define PICOPAL_SEEK_SET     0    
#define PICOPAL_SEEK_CUR     1    
#define PICOPAL_SEEK_END     2    


typedef enum {PICOPAL_BINARY_READ, PICOPAL_BINARY_WRITE, PICOPAL_TEXT_READ, PICOPAL_TEXT_WRITE}  picopal_access_mode;

typedef picopal_FILE * picopal_File;

extern picopal_File picopal_fopen (picopal_char fileName[], picopal_access_mode mode);

















extern picopal_File picopal_get_fnil (void);


extern  picopal_int8 picopal_is_fnil (picopal_File f);


extern pico_status_t picopal_fclose (picopal_File f);


extern picopal_uint32 picopal_flength (picopal_File f);


extern  picopal_uint8 picopal_feof (picopal_File f);


extern pico_status_t picopal_fseek (picopal_File f, picopal_uint32 offset, picopal_int8 seekmode);


extern pico_status_t picopal_fget_char (picopal_File f, picopal_char * ch);


extern picopal_objsize_t picopal_fread_bytes (picopal_File f, void * ptr, picopal_objsize_t objsize, picopal_uint32 nobj);

extern picopal_objsize_t picopal_fwrite_bytes (picopal_File f, void * ptr, picopal_objsize_t objsize, picopal_uint32 nobj);


extern pico_status_t picopal_fflush (picopal_File f);














 
 
 







void *picopal_mpr_alloc(picopal_objsize_t size);





void picopal_mpr_free(void **p);

#define PICOPAL_PROT_NONE   0    
#define PICOPAL_PROT_READ   1    
#define PICOPAL_PROT_WRITE  2    






pico_status_t picopal_mpr_protect(void *addr, picopal_objsize_t len, picopal_int16 prot);

 
picopal_double picopal_quick_exp(const picopal_double y);

 
 
 

extern void picopal_get_timer(picopal_uint32 * sec, picopal_uint32 * usec);




#endif  
