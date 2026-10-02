



























#ifndef PICOBASE_H_
#define PICOBASE_H_

#include "picoos.h"




 
#define PICOBASE_UTF8_MAXLEN    4

typedef picoos_uint8  picobase_utf8char[PICOBASE_UTF8_MAXLEN+1];   
typedef picoos_uint8  picobase_utf8;
typedef picoos_uint16 picobase_utf16;
typedef picoos_uint32 picobase_utf32;

 
 
 











picoos_int32 picobase_utf8_length(const picoos_uint8 *utf8str,
                                  const picoos_uint16 maxlen);










 

#define picobase_det_utf8_length(x)  (  ((x)<(picoos_uint8)'\200')?1:(((x)>=(picoos_uint8)'\370')?0:(((x)>=(picoos_uint8)'\360')?4:(((x)>=(picoos_uint8)'\340')?3:(((x)>=(picoos_uint8)'\300')?2:0)))) )










picoos_int32 picobase_lowercase_utf8_str (picoos_uchar utf8str[], picoos_char lowercase[], picoos_int32 lowercaseMaxLen, picoos_uint8 * done);









picoos_int32 picobase_uppercase_utf8_str (picoos_uchar utf8str[], picoos_char uppercase[], int uppercaseMaxLen, picoos_uint8 * done);












picoos_uint8 picobase_get_next_utf8char(const picoos_uint8 *utf8s,
                                        const picoos_uint32 utf8slenmax,
                                        picoos_uint32 *pos,
                                        picobase_utf8char utf8char);





picoos_uint8 picobase_get_next_utf8charpos(const picoos_uint8 *utf8s,
                                           const picoos_uint32 utf8slenmax,
                                           picoos_uint32 *pos);












picoos_uint8 picobase_get_prev_utf8char(const picoos_uint8 *utf8s,
                                        const picoos_uint32 utf8slenmin,
                                        picoos_uint32 *pos,
                                        picobase_utf8char utf8char);





picoos_uint8 picobase_get_prev_utf8charpos(const picoos_uint8 *utf8s,
                                           const picoos_uint32 utf8slenmin,
                                           picoos_uint32 *pos);









extern picoos_bool picobase_is_utf8_uppercase (picoos_uchar str[], picoos_int32 strmaxlen);








extern picoos_bool picobase_is_utf8_lowercase (picoos_uchar str[], picoos_int32 strmaxlen);



#endif  
