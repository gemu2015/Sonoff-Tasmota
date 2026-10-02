







































































































#ifndef PICOWA_H_
#define PICOWA_H_

#include "picoos.h"
#include "picodata.h"
#include "picorsrc.h"





 
#define PICOWA_MAXITEMSIZE 260


picodata_ProcessingUnit picowa_newWordAnaUnit(
        picoos_MemoryManager mm,
    picoos_Common common,
        picodata_CharBuffer cbIn,
        picodata_CharBuffer cbOut,
        picorsrc_Voice voice);



#endif  
