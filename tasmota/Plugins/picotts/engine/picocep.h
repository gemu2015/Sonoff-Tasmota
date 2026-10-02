



























#ifndef PICOCEP_H_
#define PICOCEP_H_

#include "picoos.h"
#include "picodata.h"
#include "picorsrc.h"







picodata_ProcessingUnit picocep_newCepUnit(picoos_MemoryManager mm,
        picoos_Common common, picodata_CharBuffer cbIn,
        picodata_CharBuffer cbOut, picorsrc_Voice voice);



#endif  
