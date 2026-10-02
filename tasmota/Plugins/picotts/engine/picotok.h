



















































































#ifndef PICOTOK_H_
#define PICOTOK_H_

#include "picoos.h"
#include "picodata.h"
#include "picorsrc.h"






picodata_ProcessingUnit picotok_newTokenizeUnit(
        picoos_MemoryManager mm,
        picoos_Common common,
        picodata_CharBuffer cbIn,
        picodata_CharBuffer cbOut,
        picorsrc_Voice voice);

#define PICOTOK_OUTBUF_SIZE 256




#endif  
