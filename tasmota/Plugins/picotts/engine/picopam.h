


















































#ifndef PICOPAM_H_
#define PICOPAM_H_

#include "picodata.h"
#include "picokdt.h"
#include "picokpdf.h"
#include "picoktab.h"
#include "picokdbg.h"







picodata_ProcessingUnit picopam_newPamUnit(
    picoos_MemoryManager mm,    picoos_Common common,
    picodata_CharBuffer cbIn,   picodata_CharBuffer cbOut,
    picorsrc_Voice voice);


#endif  
