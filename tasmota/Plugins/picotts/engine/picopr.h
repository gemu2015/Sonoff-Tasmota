
































#ifndef PICOPR_H_
#define PICOPR_H_

#include "picoos.h"
#include "picodata.h"
#include "picorsrc.h"





picodata_ProcessingUnit picopr_newPreprocUnit(
        picoos_MemoryManager mm,
        picoos_Common common,
        picodata_CharBuffer cbIn,
        picodata_CharBuffer cbOut,
        picorsrc_Voice voice);

#define PICOPR_OUTBUF_SIZE 256




#endif  
