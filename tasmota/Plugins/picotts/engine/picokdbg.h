




























#ifndef PICOKDBG_H_
#define PICOKDBG_H_


#include "picoos.h"
#include "picoknow.h"





 
 
 




pico_status_t picokdbg_specializeDbgKnowledgeBase(picoknow_KnowledgeBase thiz,
                                                  picoos_Common common);

typedef struct picokdbg_dbg *picokdbg_Dbg;




picokdbg_Dbg picokdbg_getDbg(picoknow_KnowledgeBase thiz);


 




picoos_uint8 picokdbg_getPhoneId(const picokdbg_Dbg dbg,
                                 const picoos_char *phsym);




picoos_char *picokdbg_getPhoneSym(const picokdbg_Dbg dbg,
                                  const picoos_uint8 phid);




#endif  
